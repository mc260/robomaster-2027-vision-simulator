#include "MainWindow.h"
#include "ImageProcessor.h"
#include "PreviewWidget.h"

#include <algorithm>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace vision {
namespace {
QLabel* addMetricCard(QHBoxLayout* row, const QString& title, const QString& objectName)
{
    auto* card = new QFrame;
    card->setProperty("card", true);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(5);
    auto* heading = new QLabel(title);
    heading->setProperty("muted", true);
    auto* value = new QLabel(QStringLiteral("—"));
    value->setObjectName(objectName);
    value->setProperty("metric", true);
    layout->addWidget(heading);
    layout->addWidget(value);
    row->addWidget(card, 1);
    return value;
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), engine_(new CameraEngine(this))
{
    setWindowTitle(QStringLiteral("RM2027 BaseVision · 基地视野"));
    setMinimumSize(760, 570);
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(22, 16, 22, 14);
    root->setSpacing(12);

    auto* heading = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("基地视野"));
    title->setObjectName(QStringLiteral("title"));
    heading->addWidget(title);
    auto* subtitle = new QLabel(QStringLiteral("RM2027 图传画质模拟"));
    subtitle->setProperty("muted", true);
    heading->addSpacing(8);
    heading->addWidget(subtitle);
    heading->addStretch();
    root->addLayout(heading);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(9);
    toolbar->addWidget(new QLabel(QStringLiteral("摄像头")));
    deviceBox_ = new QComboBox;
    deviceBox_->setObjectName(QStringLiteral("deviceBox"));
    deviceBox_->setMinimumWidth(180);
    deviceBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbar->addWidget(deviceBox_, 1);
    refreshButton_ = new QPushButton(QStringLiteral("刷新"));
    refreshButton_->setObjectName(QStringLiteral("refreshButton"));
    toolbar->addWidget(refreshButton_);
    toolbar->addSpacing(7);
    toolbar->addWidget(new QLabel(QStringLiteral("目标帧率")));
    fpsSpin_ = new QSpinBox;
    fpsSpin_->setObjectName(QStringLiteral("fpsSpin"));
    fpsSpin_->setRange(1, 240);
    fpsSpin_->setValue(60);
    fpsSpin_->setSuffix(QStringLiteral(" FPS"));
    fpsSpin_->setMinimumWidth(104);
    fpsSpin_->setToolTip(QStringLiteral("停止摄像头后可修改；作为采集请求值和预览刷新上限。"));
    toolbar->addWidget(fpsSpin_);
    startButton_ = new QPushButton(QStringLiteral("启动摄像头"));
    startButton_->setObjectName(QStringLiteral("startButton"));
    startButton_->setProperty("primary", true);
    toolbar->addWidget(startButton_);
    stopButton_ = new QPushButton(QStringLiteral("停止"));
    stopButton_->setObjectName(QStringLiteral("stopButton"));
    toolbar->addWidget(stopButton_);
    root->addLayout(toolbar);

    auto* viewer = new QFrame;
    viewer->setObjectName(QStringLiteral("viewer"));
    auto* viewerLayout = new QVBoxLayout(viewer);
    viewerLayout->setContentsMargins(1, 1, 1, 1);
    viewerLayout->setSpacing(0);
    auto* viewHeader = new QHBoxLayout;
    viewHeader->setContentsMargins(14, 7, 12, 7);
    modeLabel_ = new QLabel;
    modeLabel_->setObjectName(QStringLiteral("modeLabel"));
    viewHeader->addWidget(modeLabel_);
    viewHeader->addStretch();
    auto* baseLabel = new QLabel(QStringLiteral("基准 1920 × 1080"));
    baseLabel->setProperty("muted", true);
    viewHeader->addWidget(baseLabel);
    viewHeader->addSpacing(12);
    pixelButton_ = new QPushButton(QStringLiteral("1:1 像素检查"));
    pixelButton_->setObjectName(QStringLiteral("pixelButton"));
    pixelButton_->setCheckable(true);
    pixelButton_->setToolTip(QStringLiteral("一图像像素对应一屏幕像素。画面放不下时用滚动条查看，避免窗口缩放掩盖画质差异。"));
    viewHeader->addWidget(pixelButton_);
    originalButton_ = new QPushButton;
    originalButton_->setObjectName(QStringLiteral("originalButton"));
    originalButton_->setCheckable(true);
    viewHeader->addWidget(originalButton_);
    viewerLayout->addLayout(viewHeader);
    preview_ = new PreviewWidget;
    auto* previewArea = new QScrollArea;
    previewArea->setObjectName(QStringLiteral("previewScrollArea"));
    previewArea->setFrameShape(QFrame::NoFrame);
    previewArea->setWidgetResizable(true);
    previewArea->setAlignment(Qt::AlignCenter);
    previewArea->setMinimumSize(320, 120);
    previewArea->setWidget(preview_);
    viewerLayout->addWidget(previewArea, 1);
    root->addWidget(viewer, 1);

    auto* healthRow = new QHBoxLayout;
    auto* healthTitle = new QLabel(QStringLiteral("己方基地血量"));
    healthTitle->setObjectName(QStringLiteral("healthTitle"));
    healthRow->addWidget(healthTitle);
    healthRow->addStretch();
    healthSpin_ = new QSpinBox;
    healthSpin_->setObjectName(QStringLiteral("healthSpin"));
    healthSpin_->setRange(0, MaxHealth);
    healthSpin_->setSingleStep(1);
    healthSpin_->setValue(MaxHealth);
    healthSpin_->setSuffix(QStringLiteral(" / 10000"));
    healthSpin_->setMinimumWidth(166);
    healthRow->addWidget(healthSpin_);
    root->addLayout(healthRow);
    healthSlider_ = new QSlider(Qt::Horizontal);
    healthSlider_->setObjectName(QStringLiteral("healthSlider"));
    healthSlider_->setRange(0, MaxHealth);
    healthSlider_->setSingleStep(1);
    healthSlider_->setPageStep(1);
    healthSlider_->setValue(MaxHealth);
    healthSlider_->setAccessibleName(QStringLiteral("己方基地血量"));
    root->addWidget(healthSlider_);
    auto* scale = new QHBoxLayout;
    for (int value : {0, 5000, 10000}) {
        auto* tick = new QLabel(QString::number(value));
        tick->setProperty("muted", true);
        if (value == 5000) scale->addStretch();
        if (value == 10000) scale->addStretch();
        scale->addWidget(tick);
    }
    root->addLayout(scale);

    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(10);
    lambdaLabel_ = addMetricCard(metrics, QStringLiteral("后勤系数 λ"), QStringLiteral("lambdaValue"));
    resolutionLabel_ = addMetricCard(metrics, QStringLiteral("降采样分辨率"), QStringLiteral("resolutionValue"));
    captureLabel_ = addMetricCard(metrics, QStringLiteral("实际采集尺寸"), QStringLiteral("captureValue"));
    fpsLabel_ = addMetricCard(metrics, QStringLiteral("预览 FPS / 目标"), QStringLiteral("fpsValue"));
    root->addLayout(metrics);
    statusLabel_ = new QLabel;
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    statusLabel_->setWordWrap(true);
    statusLabel_->setMinimumHeight(24);
    root->addWidget(statusLabel_);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background: #101720; color: #e6edf5; font-size: 14px; }
        QLabel { background: transparent; }
        QLabel#title { font-size: 26px; font-weight: 700; }
        QLabel#healthTitle { font-size: 16px; font-weight: 600; }
        QLabel[muted="true"] { color: #91a0b3; font-size: 12px; }
        QLabel[metric="true"] { font-size: 21px; font-weight: 600; color: #e8f3ff; }
        QFrame[card="true"] { background: #182330; border: 1px solid #2b3a4b; border-radius: 9px; }
        QFrame#viewer { background: #182330; border: 1px solid #2b3a4b; border-radius: 6px; }
        QComboBox, QSpinBox { background: #182330; border: 1px solid #35465b;
                            border-radius: 6px; padding: 7px 9px; min-height: 20px; }
        QComboBox:disabled, QSpinBox:disabled { color: #6e7c8e; background: #141d28; }
        QComboBox QAbstractItemView { background: #182330; selection-background-color: #28534b; }
        QPushButton { background: #202e3e; border: 1px solid #35465b; border-radius: 6px;
                      padding: 8px 13px; min-height: 18px; }
        QPushButton:hover { background: #2a3d51; border-color: #60758e; }
        QPushButton:disabled { color: #647184; background: #17212d; border-color: #293647; }
        QPushButton[primary="true"] { background: #318c72; border-color: #45b693; color: white; }
        QPushButton[primary="true"]:hover { background: #38a182; }
        QPushButton[primary="true"]:disabled { background: #234a41; color: #7b9f93; border-color: #31574c; }
        QPushButton:checked { background: #254d67; border-color: #508eb6; }
        QSlider::groove:horizontal { background: #2c3c4f; height: 7px; border-radius: 3px; }
        QSlider::sub-page:horizontal { background: #48ba9a; border-radius: 3px; }
        QSlider::handle:horizontal { background: #dcfff3; border: 2px solid #48ba9a;
                                    width: 16px; height: 16px; margin: -6px 0; border-radius: 10px; }
        QToolTip { background: #253345; color: #e6edf5; border: 1px solid #60758e; }
    )"));

    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::startCamera);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::stopCamera);
    connect(healthSlider_, &QSlider::valueChanged, healthSpin_, &QSpinBox::setValue);
    connect(healthSpin_, &QSpinBox::valueChanged, this, [this](int value) {
        const QSignalBlocker blocker(healthSlider_);
        healthSlider_->setValue(value);
        updateRuleLabels();
        engine_->setHealth(value);
    });
    connect(fpsSpin_, &QSpinBox::valueChanged, this, [this] {
        if (state_ == State::Stopped) fpsLabel_->setText(QStringLiteral("— / %1").arg(fpsSpin_->value()));
    });
    connect(originalButton_, &QPushButton::toggled, this, [this](bool original) {
        engine_->setOriginal(original);
        updateModeLabels();
    });
    connect(pixelButton_, &QPushButton::toggled, this, [this](bool enabled) {
        preview_->setPixelInspection(enabled);
        updateModeLabels();
    });
    connect(preview_, &PreviewWidget::framePainted, this, [this] {
        if (state_ == State::Running) frameTimes_.push_back(Clock::now());
    });
    connect(engine_, &CameraEngine::opened, this, [this](QSize size, double reportedFps) {
        if (state_ == State::Stopping) return;
        setState(State::Running);
        captureLabel_->setText(QStringLiteral("%1 × %2").arg(size.width()).arg(size.height()));
        metricsStarted_ = Clock::now();
        lastMetricUpdate_ = metricsStarted_;
        const QString rate = reportedFps > 0 ? QStringLiteral("设备报告 %1 FPS").arg(reportedFps, 0, 'f', 1)
                                             : QStringLiteral("设备未报告帧率");
        setStatus(QStringLiteral("正在预览 · 目标 %1 FPS · %2").arg(activeTargetFps_).arg(rate));
    });
    connect(engine_, &CameraEngine::failed, this, [this](const QString& error) {
        lastError_ = error;
        setStatus(error, true);
        setState(State::Stopping);
        preview_->clear(QStringLiteral("摄像头未启动"));
    });
    connect(engine_, &CameraEngine::finished, this, [this] {
        engine_->joinFinished();
        previewTimer_->stop();
        setState(State::Stopped);
        preview_->clear();
        captureLabel_->setText(QStringLiteral("—"));
        fpsLabel_->setText(QStringLiteral("— / %1").arg(fpsSpin_->value()));
        frameTimes_.clear();
        if (lastError_.isEmpty()) setStatus(QStringLiteral("摄像头未启动。请选择设备后点击“启动摄像头”。"));
        else setStatus(lastError_, true);
        if (closeWhenStopped_) QTimer::singleShot(0, this, &QWidget::close);
    });

    previewTimer_ = new QTimer(this);
    previewTimer_->setTimerType(Qt::PreciseTimer);
    connect(previewTimer_, &QTimer::timeout, this, &MainWindow::pollPreview);
    updateRuleLabels();
    updateModeLabels();
    fpsLabel_->setText(QStringLiteral("— / 60"));
    refreshDevices();
    const QSize available = screen()->availableGeometry().size();
    resize(QSize(std::min(1280, static_cast<int>(available.width() * 0.90)),
                 std::min(940, static_cast<int>(available.height() * 0.88))));
}

void MainWindow::setStatus(const QString& message, bool error)
{
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(error ? QStringLiteral("color: #ffb1a8;")
                                      : QStringLiteral("color: #a1b1c5;"));
}

void MainWindow::setState(State state)
{
    state_ = state;
    const bool stopped = state == State::Stopped;
    deviceBox_->setEnabled(stopped);
    refreshButton_->setEnabled(stopped);
    fpsSpin_->setEnabled(stopped);
    startButton_->setEnabled(stopped && !devices_.isEmpty());
    stopButton_->setEnabled(state == State::Starting || state == State::Running);
}

void MainWindow::refreshDevices()
{
    if (state_ != State::Stopped) return;
    const auto previousId = deviceBox_->currentData().toString();
    QString error;
    devices_ = enumerateCameras(&error);
    deviceBox_->clear();
    for (const auto& device : devices_) deviceBox_->addItem(device.name, device.id);
    const int previousIndex = deviceBox_->findData(previousId);
    if (previousIndex >= 0) deviceBox_->setCurrentIndex(previousIndex);
    setState(State::Stopped);
    setStatus(error.isEmpty() ? QStringLiteral("摄像头未启动。请选择设备后点击“启动摄像头”。") : error,
              !error.isEmpty());
}

void MainWindow::startCamera()
{
    if (state_ != State::Stopped || deviceBox_->currentIndex() < 0) return;
    lastError_.clear();
    displayedRevision_ = 0;
    displayedSequence_ = 0;
    frameTimes_.clear();
    nextDisplay_ = Clock::now();
    activeTargetFps_ = fpsSpin_->value();
    setState(State::Starting);
    preview_->clear(QStringLiteral("正在启动摄像头…"));
    setStatus(QStringLiteral("正在请求 1920 × 1080、%1 FPS，设备不支持时将自动回退…").arg(activeTargetFps_));
    engine_->setHealth(healthSpin_->value());
    engine_->setOriginal(originalButton_->isChecked());
    engine_->start(devices_[deviceBox_->currentIndex()], activeTargetFps_);
    // Poll a bounded mailbox; no signal carrying every frame can accumulate.
    previewTimer_->start(activeTargetFps_ > 120 ? 1 : 4);
}

void MainWindow::stopCamera()
{
    if (state_ != State::Starting && state_ != State::Running) return;
    setState(State::Stopping);
    previewTimer_->stop();
    preview_->clear();
    setStatus(QStringLiteral("正在释放摄像头…"));
    engine_->requestStop();
}

void MainWindow::updateRuleLabels()
{
    const auto rule = ruleParameters(healthSpin_->value());
    lambdaLabel_->setText(QString::number(rule.lambda, 'f', 5));
    resolutionLabel_->setText(QStringLiteral("%1 × %2").arg(rule.reducedSize.width).arg(rule.reducedSize.height));
}

void MainWindow::updateModeLabels()
{
    const bool original = originalButton_->isChecked();
    const QString mode = original ? QStringLiteral("原始基准画面 · 未降质") : QStringLiteral("处理后画面");
    modeLabel_->setText(mode + (pixelButton_->isChecked() ? QStringLiteral(" · 1:1") : QString()));
    originalButton_->setText(original ? QStringLiteral("返回处理画面") : QStringLiteral("查看原始画面"));
}

void MainWindow::pollPreview()
{
    if (state_ != State::Running) return;
    const auto now = Clock::now();
    const auto frame = engine_->latestPreview();
    if (frame && frame->revision != displayedRevision_ &&
        frame->health == healthSpin_->value() && frame->original == originalButton_->isChecked()) {
        const bool newSourceFrame = frame->sourceSequence != displayedSequence_;
        if (!newSourceFrame || now >= nextDisplay_) {
            preview_->setImage(frame->image, frame->sourceSequence);
            displayedRevision_ = frame->revision;
            if (newSourceFrame) {
                displayedSequence_ = frame->sourceSequence;
                const auto interval = std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(1.0 / activeTargetFps_));
                // Keep a steady cadence instead of adding a polling tick to
                // every interval. Do not accumulate missed display deadlines.
                nextDisplay_ += interval;
                if (nextDisplay_ <= now) nextDisplay_ = now + interval;
            }
        }
    }
    while (!frameTimes_.empty() && now - frameTimes_.front() >= std::chrono::seconds(1))
        frameTimes_.pop_front();
    if (now - lastMetricUpdate_ >= std::chrono::milliseconds(250)) {
        const double elapsed = std::chrono::duration<double>(now - metricsStarted_).count();
        if (elapsed >= 0.5) {
            const double actual = frameTimes_.size() / std::min(1.0, elapsed);
            fpsLabel_->setText(QStringLiteral("%1 / %2").arg(actual, 0, 'f', 1).arg(activeTargetFps_));
        }
        lastMetricUpdate_ = now;
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (state_ == State::Stopped) {
        event->accept();
    } else {
        closeWhenStopped_ = true;
        stopCamera();
        event->ignore();
    }
}
} // namespace vision
