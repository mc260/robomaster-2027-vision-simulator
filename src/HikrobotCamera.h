#pragma once

#include "CameraDevices.h"

#include <memory>
#include <opencv2/core.hpp>

namespace vision {
// Uses the installed MVS runtime, without linking or redistributing vendor DLLs.
QVector<CameraDevice> enumerateHikrobotCameras(QString* error = nullptr);

class HikrobotCamera {
public:
    HikrobotCamera();
    ~HikrobotCamera();
    HikrobotCamera(const HikrobotCamera&) = delete;
    HikrobotCamera& operator=(const HikrobotCamera&) = delete;
    void open(const CameraDevice& device, int targetFps);
    // False means a short polling timeout. Other SDK errors throw.
    bool read(cv::Mat& image);
    double reportedFps() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace vision
