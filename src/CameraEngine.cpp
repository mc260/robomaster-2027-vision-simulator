#include "CameraEngine.h"
#include "ImageProcessor.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

namespace vision {
using Clock = std::chrono::steady_clock;

CameraEngine::CameraEngine(QObject* parent) : QObject(parent) {}

CameraEngine::~CameraEngine()
{
    requestStop();
    joinFinished();
}

void CameraEngine::start(const CameraDevice& device, int targetFps)
{
    joinFinished(); // The owner only calls start after receiving finished().
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        latestInput_.reset();
        latestOutput_.reset();
    }
    failureReported_ = false;
    stop_ = false;
    targetFps = std::clamp(targetFps, 1, 240);
    processingThread_ = std::thread(&CameraEngine::processingLoop, this, targetFps);
    captureThread_ = std::thread(&CameraEngine::captureLoop, this, device, targetFps);
}

void CameraEngine::requestStop()
{
    stop_ = true;
    changed_.notify_all();
}

void CameraEngine::joinFinished()
{
    if (captureThread_.joinable()) captureThread_.join();
    if (processingThread_.joinable()) processingThread_.join();
}

void CameraEngine::setHealth(int health)
{
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        health_ = std::clamp(health, 0, MaxHealth);
        ++settingsRevision_;
    }
    changed_.notify_all();
}

void CameraEngine::setOriginal(bool original)
{
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        original_ = original;
        ++settingsRevision_;
    }
    changed_.notify_all();
}

std::shared_ptr<const PreviewFrame> CameraEngine::latestPreview() const
{
    const std::lock_guard<std::mutex> lock(mutex_);
    return latestOutput_;
}

void CameraEngine::reportFailure(const QString& message)
{
    if (!failureReported_.exchange(true)) emit failed(message);
    requestStop();
}

void CameraEngine::captureLoop(CameraDevice device, int targetFps)
{
    try {
        cv::VideoCapture capture;
        auto openDevice = [&]() -> bool {
#ifdef Q_OS_WIN
            // Re-enumerate immediately before opening to handle changed indices.
            const auto devices = enumerateCameras();
            const auto match = std::find_if(devices.cbegin(), devices.cend(),
                [&device](const CameraDevice& candidate) { return candidate.id == device.id; });
            return match != devices.cend() && capture.open(match->backendIndex, cv::CAP_DSHOW);
#else
            return capture.open(device.id.toStdString(), cv::CAP_V4L2);
#endif
        };
        if (!openDevice()) {
            if (!stop_) reportFailure(QStringLiteral("无法打开摄像头。请检查设备是否被占用、系统摄像头权限及连接状态。"));
        } else if (!stop_) {
            capture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
            capture.set(cv::CAP_PROP_FRAME_WIDTH, BaseWidth);
            capture.set(cv::CAP_PROP_FRAME_HEIGHT, BaseHeight);
            capture.set(cv::CAP_PROP_FPS, targetFps);
            // Two driver buffers keep acquisition running while one is decoded.
            // A single V4L2 buffer halves delivery on some UVC cameras. The
            // application's latest-frame mailboxes remain bounded to one each.
            capture.set(cv::CAP_PROP_BUFFERSIZE, 2); // Best effort, backend-dependent.

            cv::Mat first;
            bool hasFirst = !stop_ && capture.read(first) && !first.empty();
            if (!hasFirst && !stop_) {
                // Some backends cannot stream after rejecting a requested mode.
                // Reopen with the camera's working default resolution/format.
                capture.release();
                if (openDevice()) {
                    capture.set(cv::CAP_PROP_FPS, targetFps);
                    capture.set(cv::CAP_PROP_BUFFERSIZE, 2);
                    hasFirst = !stop_ && capture.read(first) && !first.empty();
                }
            }
            if (!hasFirst) {
                if (!stop_) reportFailure(QStringLiteral("摄像头未返回有效画面，请停止其他占用摄像头的程序或更换设备后重试。"));
            } else if (!stop_) {
                emit opened(QSize(first.cols, first.rows), capture.get(cv::CAP_PROP_FPS));
                std::uint64_t sequence = 0;
                auto publish = [this, &sequence](cv::Mat image) {
                    if (image.type() == CV_8UC4) cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
                    else if (image.type() == CV_8UC1) cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
                    if (image.type() != CV_8UC3)
                        throw std::runtime_error("Unsupported camera pixel format");
                    auto frame = std::make_shared<RawFrame>(RawFrame{std::move(image), ++sequence});
                    {
                        const std::lock_guard<std::mutex> lock(mutex_);
                        latestInput_ = std::move(frame);
                    }
                    changed_.notify_one();
                };
                publish(std::move(first));
                while (!stop_) {
                    cv::Mat frame;
                    if (!capture.read(frame) || frame.empty()) {
                        if (!stop_) reportFailure(QStringLiteral("摄像头读取失败或连接已断开。请检查设备后重新点击启动。"));
                        break;
                    }
                    if (!stop_) publish(std::move(frame));
                }
            }
            capture.release();
        }
    } catch (const std::exception& error) {
        if (!stop_) reportFailure(QStringLiteral("摄像头异常：%1").arg(QString::fromUtf8(error.what())));
    }
    requestStop();
    if (processingThread_.joinable()) processingThread_.join();
    emit finished();
}

void CameraEngine::processingLoop(int targetFps)
{
    const auto interval = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / targetFps));
    auto nextFrame = Clock::now();
    std::uint64_t lastSequence = 0;
    std::uint64_t lastSettings = 0;
    std::uint64_t outputRevision = 0;
    cv::Mat baseline;
    try {
        while (!stop_) {
            std::shared_ptr<const RawFrame> input;
            int health;
            bool original;
            std::uint64_t settings;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait(lock, [&] {
                    return stop_ || (latestInput_ &&
                        (latestInput_->sequence != lastSequence || settingsRevision_ != lastSettings));
                });
                if (stop_) break;
                if (settingsRevision_ == lastSettings && Clock::now() < nextFrame) {
                    changed_.wait_until(lock, nextFrame, [&] {
                        return stop_ || settingsRevision_ != lastSettings;
                    });
                    if (stop_) break;
                }
                input = latestInput_;
                health = health_;
                original = original_;
                settings = settingsRevision_;
            }
            const auto processingStarted = Clock::now();
            if (input->sequence != lastSequence)
                baseline = makeBaseline(input->image);
            const cv::Mat result = original ? baseline : degrade(baseline, health);
            QImage image(result.data, result.cols, result.rows,
                         static_cast<qsizetype>(result.step), QImage::Format_BGR888);
            auto preview = std::make_shared<PreviewFrame>();
            preview->image = image.copy(); // No borrowed cv::Mat memory crosses threads.
            preview->sourceSequence = input->sequence;
            preview->revision = ++outputRevision;
            preview->health = health;
            preview->original = original;
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                latestOutput_ = std::move(preview);
            }
            lastSequence = input->sequence;
            lastSettings = settings;
            nextFrame = processingStarted + interval;
        }
    } catch (const std::exception& error) {
        if (!stop_) reportFailure(QStringLiteral("图像处理失败：%1").arg(QString::fromUtf8(error.what())));
    }
}
} // namespace vision
