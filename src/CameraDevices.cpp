#include "CameraDevices.h"
#include "HikrobotCamera.h"

#include <algorithm>
#include <QDir>
#include <QRegularExpression>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dshow.h>
#include <memory>
#endif

namespace vision {
#ifdef Q_OS_WIN
namespace {
struct ReleaseCom {
    template<class T> void operator()(T* value) const { if (value) value->Release(); }
};
template<class T> using ComPtr = std::unique_ptr<T, ReleaseCom>;
class ComApartment {
public:
    ComApartment() : result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComApartment() { if (SUCCEEDED(result)) CoUninitialize(); }
    HRESULT result;
};
QString property(IPropertyBag* bag, const wchar_t* key)
{
    VARIANT value;
    VariantInit(&value);
    QString text;
    if (SUCCEEDED(bag->Read(key, &value, nullptr)) && value.vt == VT_BSTR)
        text = QString::fromWCharArray(value.bstrVal);
    VariantClear(&value);
    return text;
}
} // namespace
#endif

static QVector<CameraDevice> enumerateSystemCameras(QString* error)
{
    QVector<CameraDevice> devices;
    if (error) error->clear();
#ifdef Q_OS_LINUX
    const QDir directory(QStringLiteral("/dev"));
    const QRegularExpression pattern(QStringLiteral("^video(\\d+)$"));
    auto files = directory.entryList({QStringLiteral("video*")}, QDir::System);
    std::sort(files.begin(), files.end(), [&pattern](const QString& a, const QString& b) {
        return pattern.match(a).captured(1).toInt() < pattern.match(b).captured(1).toInt();
    });
    for (const auto& file : files) {
        if (!pattern.match(file).hasMatch()) continue;
        const QString path = directory.absoluteFilePath(file);
        const int descriptor = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
        if (descriptor < 0) continue;
        v4l2_capability capability{};
        const int result = ::ioctl(descriptor, VIDIOC_QUERYCAP, &capability);
        ::close(descriptor);
        if (result < 0) continue;
        const auto flags = (capability.capabilities & V4L2_CAP_DEVICE_CAPS)
                               ? capability.device_caps : capability.capabilities;
        // OpenCV's V4L2 backend requires single-plane streaming capture.
        if (!(flags & V4L2_CAP_VIDEO_CAPTURE) || !(flags & V4L2_CAP_STREAMING)) continue;
        const auto name = QString::fromLocal8Bit(reinterpret_cast<const char*>(capability.card));
        devices.push_back({path, QStringLiteral("%1  ·  %2").arg(name, path), -1});
    }
#elif defined(Q_OS_WIN)
    ComApartment apartment;
    if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE) {
        if (error) *error = QStringLiteral("无法初始化 Windows 摄像头枚举服务。");
        return devices;
    }
    ICreateDevEnum* rawEnumerator = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ICreateDevEnum, reinterpret_cast<void**>(&rawEnumerator)))) {
        if (error) *error = QStringLiteral("无法创建 DirectShow 设备枚举器。");
        return devices;
    }
    ComPtr<ICreateDevEnum> enumerator(rawEnumerator);
    IEnumMoniker* rawMonikers = nullptr;
    if (enumerator->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                          &rawMonikers, 0) != S_OK)
        return devices;
    ComPtr<IEnumMoniker> monikers(rawMonikers);
    IMoniker* rawMoniker = nullptr;
    int ordinal = 0;
    while (monikers->Next(1, &rawMoniker, nullptr) == S_OK) {
        ComPtr<IMoniker> moniker(rawMoniker);
        const int index = ordinal++;
        IPropertyBag* rawBag = nullptr;
        QString name, id;
        if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                                             reinterpret_cast<void**>(&rawBag)))) {
            ComPtr<IPropertyBag> bag(rawBag);
            name = property(bag.get(), L"FriendlyName");
            id = property(bag.get(), L"DevicePath");
        }
        if (name.isEmpty()) name = QStringLiteral("摄像头 %1").arg(index + 1);
        if (id.isEmpty()) id = QStringLiteral("dshow:%1").arg(index);
        devices.push_back({id, QStringLiteral("%1  ·  #%2").arg(name).arg(index + 1), index});
    }
#else
    if (error) *error = QStringLiteral("本程序目前支持 Linux 和 Windows。");
#endif
    if (devices.isEmpty() && error && error->isEmpty())
        *error = QStringLiteral("未发现可用摄像头，请连接设备、检查摄像头权限后刷新。");
    return devices;
}

QVector<CameraDevice> enumerateCameras(QString* error)
{
    QString systemError, mvsError;
    auto devices = enumerateSystemCameras(&systemError);
    devices += enumerateHikrobotCameras(&mvsError);
    if (error) {
        error->clear();
        if (devices.isEmpty()) {
            *error = systemError;
            if (!mvsError.isEmpty()) *error += QLatin1Char('\n') + mvsError;
        }
    }
    return devices;
}
} // namespace vision
