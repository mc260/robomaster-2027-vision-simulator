#pragma once

#include <QString>
#include <QVector>

namespace vision {
struct CameraDevice {
    QString id;
    QString name;
    int backendIndex = -1; // DirectShow's original moniker ordinal, not a UI row.
};

QVector<CameraDevice> enumerateCameras(QString* error = nullptr);
} // namespace vision
