#include "HikrobotCamera.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <QLibrary>
#include <QStringList>

#ifdef BASEVISION_HAS_MVS
#include <MvCameraControl.h>
#endif

namespace vision {
namespace {
[[noreturn]] void fail(const QString& message)
{
    throw std::runtime_error(message.toUtf8().constData());
}

#ifdef BASEVISION_HAS_MVS
QString sdkError(const QString& operation, int code)
{
    return QStringLiteral("海康相机：%1失败（MVS 0x%2）。请检查连接、MVS 驱动及设备占用。")
        .arg(operation).arg(static_cast<unsigned int>(code), 8, 16, QLatin1Char('0'));
}

void check(int result, const QString& operation)
{
    if (result != MV_OK) fail(sdkError(operation, result));
}

// SDK structs come from official installed headers; no copied ABI definitions.
class MvsSdk {
public:
#define MVS_FUNCTIONS(X) \
    X(MV_CC_Initialize) X(MV_CC_Finalize) X(MV_CC_EnumDevices) \
    X(MV_CC_CreateHandle) X(MV_CC_DestroyHandle) X(MV_CC_OpenDevice) X(MV_CC_CloseDevice) \
    X(MV_CC_StartGrabbing) X(MV_CC_StopGrabbing) X(MV_CC_GetImageBuffer) X(MV_CC_FreeImageBuffer) \
    X(MV_CC_ConvertPixelTypeEx) X(MV_CC_SetEnumValueByString) \
    X(MV_CC_GetIntValueEx) X(MV_CC_SetIntValueEx) \
    X(MV_CC_GetFloatValue) X(MV_CC_SetFloatValue) X(MV_CC_SetBoolValue) \
    X(MV_CC_SetImageNodeNum) X(MV_CC_SetGrabStrategy) X(MV_CC_IsDeviceConnected)
#define DECLARE_MVS(name) decltype(&::name) name = nullptr;
    MVS_FUNCTIONS(DECLARE_MVS)
#undef DECLARE_MVS

    static MvsSdk& instance() { static MvsSdk sdk; return sdk; }

    bool load(QString* error)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) return true;
        QStringList candidates;
        const QString explicitLibrary = qEnvironmentVariable("BASEVISION_MVS_LIBRARY");
        if (!explicitLibrary.isEmpty()) candidates << explicitLibrary;
        else {
#ifdef BASEVISION_MVS_LIBRARY_HINT
            candidates << QString::fromUtf8(BASEVISION_MVS_LIBRARY_HINT);
#endif
            const auto sdkRoot = qEnvironmentVariable("MVCAM_SDK_PATH");
            const auto runtimeRoot = qEnvironmentVariable("MVCAM_COMMON_RUNENV");
#ifdef Q_OS_WIN
            for (const auto& root : {runtimeRoot, sdkRoot}) {
                if (!root.isEmpty()) candidates << root + "/Win64_x64/MvCameraControl.dll"
                    << root + "/Runtime/Win64_x64/MvCameraControl.dll"
                    << root + "/MvCameraControl.dll";
            }
#else
            if (!sdkRoot.isEmpty()) candidates << sdkRoot + "/lib/64/libMvCameraControl.so";
            if (!runtimeRoot.isEmpty()) candidates << runtimeRoot + "/64/libMvCameraControl.so";
            candidates << QStringLiteral("/opt/MVS/lib/64/libMvCameraControl.so");
#endif
            candidates << QStringLiteral("MvCameraControl");
        }
        candidates.removeDuplicates();
        QString detail;
        for (const auto& path : candidates) {
            library_.setFileName(path);
            if (!library_.load()) { detail = library_.errorString(); continue; }
            bool complete = true;
#define RESOLVE_MVS(name) \
            name = reinterpret_cast<decltype(name)>(library_.resolve(#name)); \
            complete = complete && name != nullptr;
            MVS_FUNCTIONS(RESOLVE_MVS)
#undef RESOLVE_MVS
            if (!complete) {
                detail = QStringLiteral("MVS 运行库缺少所需接口，请安装匹配的 MVS 4.7 或更新版本。");
                library_.unload();
                continue;
            }
            const int result = MV_CC_Initialize();
            if (result == MV_OK) { initialized_ = true; return true; }
            detail = sdkError(QStringLiteral("初始化 SDK"), result);
            library_.unload();
        }
        if (error) *error = QStringLiteral("海康 MVS 运行库不可用：%1").arg(detail);
        return false;
    }

    ~MvsSdk() { if (initialized_) MV_CC_Finalize(); }

    // EnumDevices owns its result storage. Copy descriptors before another enumeration.
    QVector<MV_CC_DEVICE_INFO> devices()
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        MV_CC_DEVICE_INFO_LIST list{};
        check(MV_CC_EnumDevices(MV_USB_DEVICE, &list), QStringLiteral("枚举 USB 设备"));
        QVector<MV_CC_DEVICE_INFO> result;
        for (unsigned int i = 0; i < list.nDeviceNum; ++i)
            if (list.pDeviceInfo[i]) result.push_back(*list.pDeviceInfo[i]);
        return result;
    }

private:
    QLibrary library_;
    std::mutex mutex_;
    bool initialized_ = false;
};
#undef MVS_FUNCTIONS

template<std::size_t N> QString deviceText(const unsigned char (&text)[N])
{
    const auto* begin = reinterpret_cast<const char*>(text);
    const auto* end = std::find(begin, begin + N, '\0');
    return QString::fromUtf8(begin, static_cast<qsizetype>(end - begin));
}

QString deviceId(const MV_CC_DEVICE_INFO& info)
{
    const auto& usb = info.SpecialInfo.stUsb3VInfo;
    const auto serial = deviceText(usb.chSerialNumber);
    return QStringLiteral("mvs:usb:%1").arg(serial.isEmpty() ? deviceText(usb.chDeviceGUID) : serial);
}
#endif
} // namespace

QVector<CameraDevice> enumerateHikrobotCameras(QString* error)
{
    if (error) error->clear();
    QVector<CameraDevice> result;
#ifdef BASEVISION_HAS_MVS
    auto& sdk = MvsSdk::instance();
    if (!sdk.load(error)) return result;
    try {
        for (const auto& info : sdk.devices()) {
            const auto& usb = info.SpecialInfo.stUsb3VInfo;
            result.push_back({deviceId(info), QStringLiteral("海康 MVS · %1 · %2")
                .arg(deviceText(usb.chModelName), deviceText(usb.chSerialNumber)),
                -1, CameraBackend::HikrobotMvs});
        }
    } catch (const std::exception& exception) {
        if (error) *error = QString::fromUtf8(exception.what());
    }
#else
    if (error) *error = QStringLiteral("当前构建未启用海康 MVS。安装 MVS 开发包后重新构建（见 docs/BUILDING.md）。");
#endif
    return result;
}

struct HikrobotCamera::Impl {
#ifdef BASEVISION_HAS_MVS
    MvsSdk& sdk = MvsSdk::instance();
    void* handle = nullptr;
    bool opened = false, grabbing = false;
    ~Impl()
    {
        if (grabbing) sdk.MV_CC_StopGrabbing(handle);
        if (opened) sdk.MV_CC_CloseDevice(handle);
        if (handle) sdk.MV_CC_DestroyHandle(handle);
    }
#endif
    double fps = 0;
};

HikrobotCamera::HikrobotCamera() : impl_(std::make_unique<Impl>()) {}
HikrobotCamera::~HikrobotCamera() = default;

void HikrobotCamera::open(const CameraDevice& device, int targetFps)
{
#ifdef BASEVISION_HAS_MVS
    impl_ = std::make_unique<Impl>();
    auto& state = *impl_;
    auto& sdk = state.sdk;
    QString error;
    if (!sdk.load(&error)) fail(error);
    const auto devices = sdk.devices();
    const auto found = std::find_if(devices.cbegin(), devices.cend(),
        [&device](const MV_CC_DEVICE_INFO& info) { return deviceId(info) == device.id; });
    if (found == devices.cend()) fail(QStringLiteral("所选海康相机已断开，请检查 USB 连接后刷新设备。"));
    check(sdk.MV_CC_CreateHandle(&state.handle, &*found), QStringLiteral("创建设备句柄"));
    check(sdk.MV_CC_OpenDevice(state.handle, MV_ACCESS_Exclusive, 0), QStringLiteral("打开设备"));
    state.opened = true;
    check(sdk.MV_CC_SetEnumValueByString(state.handle, "TriggerMode", "Off"), QStringLiteral("关闭触发模式"));
    check(sdk.MV_CC_SetEnumValueByString(state.handle, "AcquisitionMode", "Continuous"), QStringLiteral("设置连续采集"));
    // Width/Height on industrial cameras are normally a sensor ROI. Requesting
    // 1920x1080 here would crop the scene. Preserve full sensor coverage instead.
    for (const char* key : {"OffsetX", "OffsetY", "Width", "Height"}) {
        MVCC_INTVALUE_EX value{};
        if (sdk.MV_CC_GetIntValueEx(state.handle, key, &value) == MV_OK)
            sdk.MV_CC_SetIntValueEx(state.handle, key, key[0] == 'O' ? value.nMin : value.nMax);
    }
    sdk.MV_CC_SetBoolValue(state.handle, "AcquisitionFrameRateEnable", true);
    MVCC_FLOATVALUE rate{};
    if (sdk.MV_CC_GetFloatValue(state.handle, "AcquisitionFrameRate", &rate) == MV_OK) {
        sdk.MV_CC_SetFloatValue(state.handle, "AcquisitionFrameRate",
            std::clamp(static_cast<float>(targetFps), rate.fMin, rate.fMax));
    }
    if (sdk.MV_CC_GetFloatValue(state.handle, "ResultingFrameRate", &rate) == MV_OK ||
        sdk.MV_CC_GetFloatValue(state.handle, "AcquisitionFrameRate", &rate) == MV_OK)
        state.fps = rate.fCurValue;
    check(sdk.MV_CC_SetImageNodeNum(state.handle, 3), QStringLiteral("设置采集缓冲"));
    check(sdk.MV_CC_SetGrabStrategy(state.handle, MV_GrabStrategy_LatestImagesOnly), QStringLiteral("设置最新帧策略"));
    check(sdk.MV_CC_StartGrabbing(state.handle), QStringLiteral("启动采集"));
    state.grabbing = true;
#else
    Q_UNUSED(device);
    Q_UNUSED(targetFps);
    fail(QStringLiteral("当前构建未启用海康 MVS，请安装 SDK 后重新构建。"));
#endif
}

bool HikrobotCamera::read(cv::Mat& image)
{
#ifdef BASEVISION_HAS_MVS
    auto& state = *impl_;
    auto& sdk = state.sdk;
    MV_FRAME_OUT frame{};
    const int result = sdk.MV_CC_GetImageBuffer(state.handle, &frame, 200);
    if (static_cast<unsigned int>(result) == MV_E_NODATA) {
        if (!sdk.MV_CC_IsDeviceConnected(state.handle)) fail(QStringLiteral("海康相机连接已断开，请检查设备后重新点击启动。"));
        return false;
    }
    check(result, QStringLiteral("读取图像"));
    struct ReturnBuffer {
        Impl& state;
        MV_FRAME_OUT& frame;
        ~ReturnBuffer() { state.sdk.MV_CC_FreeImageBuffer(state.handle, &frame); }
    } guard{state, frame};
    const auto& info = frame.stFrameInfo;
    const unsigned int width = info.nExtendWidth ? info.nExtendWidth : info.nWidth;
    const unsigned int height = info.nExtendHeight ? info.nExtendHeight : info.nHeight;
    const auto bytes = static_cast<std::uint64_t>(width) * height * 3;
    if (!frame.pBufAddr || !width || !height || width > INT_MAX || height > INT_MAX ||
        bytes > std::numeric_limits<unsigned int>::max())
        fail(QStringLiteral("海康相机返回了无效的图像尺寸。"));
    image.create(static_cast<int>(height), static_cast<int>(width), CV_8UC3);
    MV_CC_PIXEL_CONVERT_PARAM_EX conversion{};
    conversion.nWidth = width;
    conversion.nHeight = height;
    conversion.enSrcPixelType = info.enPixelType;
    conversion.pSrcData = frame.pBufAddr;
    conversion.nSrcDataLen = info.nFrameLen;
    conversion.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
    conversion.pDstBuffer = image.data;
    conversion.nDstBufferSize = static_cast<unsigned int>(bytes);
    check(sdk.MV_CC_ConvertPixelTypeEx(state.handle, &conversion), QStringLiteral("转换图像像素格式"));
    if (conversion.nDstLen != bytes) fail(QStringLiteral("海康相机像素转换返回的图像长度不完整。"));
    // image owns its memory; the SDK buffer is returned before leaving read().
    return true;
#else
    Q_UNUSED(image);
    fail(QStringLiteral("当前构建未启用海康 MVS。"));
#endif
}

double HikrobotCamera::reportedFps() const { return impl_->fps; }
} // namespace vision
