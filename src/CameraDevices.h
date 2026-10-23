#pragma once

#include <QString>
#include <QVector>

namespace vision {
enum class CameraBackend { System, HikrobotMvs };

struct CameraDevice {
    QString id;
    QString name;
    int backendIndex = -1; // DirectShow's original moniker ordinal, not a UI row.
    CameraBackend backend = CameraBackend::System;
};

QVector<CameraDevice> enumerateCameras(QString* error = nullptr);
} // namespace vision
