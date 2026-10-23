#pragma once

#include "CameraDevices.h"

#include <QImage>
#include <QObject>
#include <QSize>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <opencv2/core.hpp>

namespace vision {
struct PreviewFrame {
    QImage image;
    std::uint64_t sourceSequence = 0;
    std::uint64_t revision = 0;
    int health = 10000;
    bool original = false;
};

// Exactly one latest input and one latest output, never a queue of frames.
class CameraEngine : public QObject {
    Q_OBJECT
public:
    explicit CameraEngine(QObject* parent = nullptr);
    ~CameraEngine() override;
    void start(const CameraDevice& device, int targetFps);
    void requestStop(); // Nonblocking, including while the driver is opening.
    void joinFinished();
    void setHealth(int health);
    void setOriginal(bool original);
    std::shared_ptr<const PreviewFrame> latestPreview() const;

signals:
    void opened(QSize actualSize, double reportedFps);
    void failed(QString message);
    void finished();

private:
    struct RawFrame { cv::Mat image; std::uint64_t sequence; };
    void captureLoop(CameraDevice device, int targetFps);
    void captureSystemCamera(const CameraDevice& device, int targetFps);
    void captureHikrobotCamera(const CameraDevice& device, int targetFps);
    void publishInput(cv::Mat image, std::uint64_t& sequence);
    void processingLoop(int targetFps);
    void reportFailure(const QString& message);

    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::shared_ptr<const RawFrame> latestInput_;
    std::shared_ptr<const PreviewFrame> latestOutput_;
    int health_ = 10000;
    bool original_ = false;
    std::uint64_t settingsRevision_ = 0;
    std::atomic<bool> stop_{true};
    std::atomic<bool> failureReported_{false};
    std::thread captureThread_;
    std::thread processingThread_;
};
} // namespace vision
