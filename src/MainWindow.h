#pragma once

#include "CameraEngine.h"

#include <QMainWindow>
#include <chrono>
#include <deque>

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

namespace vision {
class PreviewWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class State { Stopped, Starting, Running, Stopping };
    using Clock = std::chrono::steady_clock;
    void refreshDevices();
    void startCamera();
    void stopCamera();
    void setState(State state);
    void updateRuleLabels();
    void updateModeLabels();
    void pollPreview();
    void setStatus(const QString& message, bool error = false);

    CameraEngine* engine_;
    QVector<CameraDevice> devices_;
    State state_ = State::Stopped;
    QComboBox* deviceBox_;
    QPushButton* refreshButton_;
    QPushButton* startButton_;
    QPushButton* stopButton_;
    QPushButton* originalButton_;
    QPushButton* pixelButton_;
    QSpinBox* fpsSpin_;
    QSpinBox* healthSpin_;
    QSlider* healthSlider_;
    QLabel* modeLabel_;
    QLabel* lambdaLabel_;
    QLabel* resolutionLabel_;
    QLabel* captureLabel_;
    QLabel* fpsLabel_;
    QLabel* statusLabel_;
    PreviewWidget* preview_;
    QTimer* previewTimer_;
    QString lastError_;
    bool closeWhenStopped_ = false;
    std::uint64_t displayedRevision_ = 0;
    std::uint64_t displayedSequence_ = 0;
    int activeTargetFps_ = 60;
    Clock::time_point nextDisplay_{};
    Clock::time_point metricsStarted_{};
    Clock::time_point lastMetricUpdate_{};
    std::deque<Clock::time_point> frameTimes_;
};
} // namespace vision
