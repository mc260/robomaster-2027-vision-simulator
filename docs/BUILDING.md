# 构建、运行与摄像头说明

C++17 桌面程序，使用 Qt 6 Widgets、OpenCV 和 CMake，支持 Linux / Windows。

本机测试详情见 [VALIDATION.md](../VALIDATION.md)。

## 当前电脑直接运行

```bash
cd robomaster-2027-vision-simulator
bash scripts/run-linux.sh
```

也可以直接运行 `build/base_vision`。程序打开后不会自动启用摄像头。

1. 在顶部选择摄像头，点击“启动摄像头”。
2. 拖动“己方基地血量”滑块，或输入 0～10000 的整数，实时观察画质变化。
3. 点击“查看原始画面”，对照降质前的相同视野；再次点击返回处理画面。
4. 先点击“停止”，再更换摄像头、刷新设备或调整目标 FPS。

检查细节时，点击“1:1 像素检查”，按图像像素与屏幕物理像素一一对应显示；画面放不下时可滚动。默认仍为自适应显示。

摄像头读取失败会停止采集并显示原因，需手动重新启动。停止后清空画面。程序不保存摄像头图片或录像。

## 规则与分辨率

依据 RMUC 2027 规则 V1.0.0 的5.6.1及表5-19，基地初始血量为10000。官方来源见 [RULES.md](RULES.md)。令当前血量为H：

```text
已损失血量 Δ = 10000 − H
后勤系数 λ = 1 − Δ / 20000 = 0.5 + H / 20000
目标总像素数 = (1920 × 1080) × λ²
中间宽度 = round(1920 × λ)
中间高度 = round(1080 × λ)
```

`λ²` 作用于总像素数，宽、高分别乘以 `λ`。

| 基地血量 | λ | 中间分辨率 | 最终处理画面 |
|---:|---:|---:|---:|
| 10000 | 1 | 1920×1080 | 1920×1080 |
| 5000 | 0.75 | 1440×810 | 1920×1080 |
| 0 | 0.5 | 960×540 | 1920×1080 |

每帧的处理顺序：

1. 摄像头输入保持比例、居中补黑边，统一为 1920×1080。缩小输入使用面积平均，放大输入使用双线性；较低分辨率输入无法恢复原本不存在的细节。
2. 以 `cv::INTER_AREA` 面积平均降采样至上述中间分辨率。
3. 以 `cv::INTER_LINEAR` 双线性放大回 1920×1080。
4. 在窗口内等比例适配显示，内部基准与降采样尺寸不随窗口大小变化。

满血且中间尺寸等于基准时直接保留基准图像，避免额外过滤。宽、高各自四舍五入到整数像素，因此任意血量下的实际像素数会有不足一个像素边长的取整误差。控件与参数文字位于视频处理区域之外，保持清晰。画面不镜像。

“原始画面”指第 1 步后的 1920×1080 基准画面，未进行血量降质，所以切换前后视野和大小一致。

## 摄像头与帧率

- 普通摄像头优先请求 1920×1080、目标 FPS；不支持时接受驱动提供的可用模式。首次读帧失败时，重开设备尝试默认分辨率与格式。
- 目标 FPS 范围 1～240，步长 1，每次打开程序默认为 60；也是处理与预览刷新上限，实际帧率可能更低。
- 当前 HP True Vision FHD Camera 的驱动声明支持 1920×1080 MJPG，最高 30 FPS；请求 60 并不意味着它能采集 60。
- “实际采集尺寸”来自真正收到的帧。“设备报告 FPS”是驱动提供的值；“预览 FPS”按最近约 1 秒内实际绘制的新采集帧统计。
- 同一采集帧因调血量、切换原图、窗口重绘而再次显示时，不重复计入 FPS。视频内容静止时，摄像头新采集的帧仍正常计数。
- 采集、处理分别运行在工作线程。输入、输出各只保留最新一帧，处理不及时时跳过旧帧，不建立无界队列。
- 摄像头驱动请求双缓冲，保证解码期间仍可接收下一帧；应用内部的最新帧缓冲仍各为一帧。
- 停止与关闭窗口会等待摄像头驱动释放资源；若驱动正阻塞读帧，界面保持响应并显示释放状态。

Linux 通过 V4L2 能力查询筛选有效单平面采集节点，过滤 metadata 节点，并直接按设备路径打开。Windows 使用 DirectShow 枚举与采集，保存原始枚举序号，打开前按设备标识重新匹配，避免将界面序号误作设备序号。

## 海康 USB 工业相机（MVS）

支持通过 MVS SDK 枚举、独占打开 USB 工业相机，设备列表显示“海康 MVS · 型号 · 序列号”。本机已实测 MV-CS060-10UC-PRO；此后端目前枚举 USB 设备。

1. 安装厂商 **MVS 4.7 或更新版本**，包括开发头文件、运行库及 USB 驱动/权限规则。先确认 MVS 客户端能够打开相机，再关闭客户端释放设备。
2. 重新构建程序。Linux 默认查找 `/opt/MVS/include`，也读取 `MVCAM_SDK_PATH`；Windows 查找 MVS 的 `Development/Includes`。自定义目录可传入 `MVS_ROOT`。
3. 启动程序，点击“刷新”，选择“海康 MVS”，设置目标 FPS，点击“启动摄像头”。更换设备或 FPS 前先停止。

Linux 自定义安装位置：

```bash
bash scripts/build-linux.sh -DMVS_ROOT=/opt/MVS
bash scripts/run-linux.sh
```

Windows 构建脚本可追加 `-MvsRoot "C:\Program Files (x86)\MVS\Development"`，其他 Qt/OpenCV 参数同下文。运行时需安装对应架构的 MVS Runtime 与驱动，让 `MvCameraControl.dll` 及依赖位于厂商配置的搜索路径；也可将运行库目录加入 `PATH`。Windows 海康采集尚未实机验证。

程序动态加载 MVS，**不随项目分发厂商 SDK**。默认支持按需检测；找不到头文件时仍能构建普通摄像头版本。构建日志应出现 `Hikrobot USB support enabled`。可用 `-DBASEVISION_ENABLE_MVS=OFF` 明确关闭。自定义运行库位置可设置环境变量 `BASEVISION_MVS_LIBRARY` 为完整的 `.so` / `.dll` 路径，相关 SDK 依赖仍需正常安装。

采集行为：

- 每次启动设置连续采集、关闭触发；按相机允许范围请求目标帧率，受曝光和传输带宽限制时实际帧率更低。保留已有曝光、增益与白平衡设置，不写入相机的永久用户配置。
- 工业相机的 `Width/Height` 通常表示传感器 ROI，直接设置 1920×1080 会裁剪视野。因此尽量将偏移归零、宽高恢复为当前模式支持的最大值，接收完整视野后等比例适配到 1920×1080，再执行同一套 `INTER_AREA → INTER_LINEAR` 血量处理。
- SDK 转换 Bayer、Mono 等输入像素格式为 BGR8，归还 SDK 缓冲后使用独立图像内存。SDK 使用 `LatestImagesOnly`，应用继续只保留最新帧。
- 每次取帧最多等待 200 ms，以便响应停止。连续 5 秒未收到图像或 SDK 报错时停止并提示原因，等待手动重启。

优先使用 USB 3.x 接口和合适的数据线。本机相机当前枚举为 **USB 2.0 / 480 Mbps**，3072×2048 全视野下实测约 6 FPS；请求 60 FPS 无法突破相机与连接的交付能力。

不保存图像的真机检查：

```bash
./build/camera_smoke --list
./build/camera_smoke --hikrobot
QT_QPA_PLATFORM=offscreen BASEVISION_TEST_CAMERA=1 BASEVISION_TEST_CAMERA_BACKEND=mvs ./build/ui_tests
```

`--hikrobot` 明确选用海康后端，检查血量、原图、停止/重启以及 60、15、1 FPS 请求。普通 `camera_smoke` 仍使用列表第一个设备。

## Linux 构建与验证

需要 CMake ≥ 3.21、C++17 编译器、Qt ≥ 6.2、OpenCV 4.x。Ubuntu / Debian 常见依赖：

```bash
sudo apt install build-essential cmake qt6-base-dev libopencv-dev
bash scripts/build-linux.sh
```

脚本执行 Release 构建和自动测试。也可以手动执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

普通测试不打开摄像头，覆盖公式端点、像素取整、黑边、插值像素结果、滑块与输入框同步、原图切换以及新帧绘制计数。

需要主动进行真实摄像头测试时，运行：

```bash
./build/camera_smoke
```

该测试打开列表中的第一个摄像头，验证默认请求 60 FPS、血量调整、原图切换、停止及以 15 FPS 重启，随后释放设备；仅打印尺寸和帧率，不保存图像。

完整桌面控件与真实摄像头联调（同样主动打开设备）：

```bash
BASEVISION_TEST_CAMERA=1 ./build/ui_tests
```

## Windows 构建

交付范围是跨平台源码、CMake、Windows 构建和部署脚本；当前工作在 Linux 完成，Windows EXE 需要在 Windows 构建并运行验证。

准备以下同为 **x64、MSVC、Release** 的依赖：

- Visual Studio 2022 的“使用 C++ 的桌面开发”（含 Windows SDK），CMake、Ninja。
- Qt 6 的 MSVC x64 版本，需有 Widgets；启用测试还需 Qt Test。
- 与所用 MSVC 兼容的 OpenCV 4.x 开发包及运行 DLL。不要将 MinGW 库与 MSVC C++ 库混用。

在 **Developer PowerShell for VS 2022 / x64** 中运行，按实际安装路径替换参数：

```powershell
.\scripts\build-windows.ps1 `
  -QtRoot "C:\Qt\6.8.3\msvc2022_64" `
  -OpenCVDir "C:\opencv\build\x64\vc16\lib" `
  -OpenCVBin "C:\opencv\build\x64\vc16\bin" `
  -RunTests
```

`QtRoot` 是包含 `bin`、`lib` 的 Qt 安装目录。`OpenCVDir` 必须直接包含 `OpenCVConfig.cmake`；`OpenCVBin` 包含该版本的 DLL。OpenCV 安装布局可能不同，请使用实际路径。构建结果位于 `build-windows/base_vision.exe`。

## Windows 部署

继续在同一个 Developer PowerShell 中执行：

```powershell
.\scripts\deploy-windows.ps1 `
  -QtRoot "C:\Qt\6.8.3\msvc2022_64" `
  -OpenCVBin "C:\opencv\build\x64\vc16\bin"
```

脚本使用 CMake 安装实际链接的运行 DLL，再运行对应 Qt 的 `windeployqt` 收集 Qt 平台插件及编译器运行依赖。输出位于 `dist/windows`，整个目录一起分发。在 Windows 干净环境验证启动、摄像头授权、设备选择、帧率回退和启停后再发布。

Windows 源码和脚本尚未在 Windows 编译或运行验证。Linux 可执行文件也依赖系统 Qt/OpenCV 库，不是无依赖便携包。

## 代码位置

```text
src/ImageProcessor.*  规则计算、黑边、降采样与放大
src/CameraDevices.*   Linux / Windows 摄像头枚举
src/HikrobotCamera.*  可选 MVS SDK 加载、海康 USB 枚举与采集
src/CameraEngine.*    采集线程、处理线程、最新帧缓冲
src/PreviewWidget.*   等比例预览与新帧绘制统计
src/MainWindow.*      桌面界面、控件同步、启停状态
tests/               规则、界面及可选摄像头测试
scripts/             Linux 与 Windows 构建、运行、部署
```

规则图片未随源码分发。第三方库的使用与分发遵循各自许可证；Windows 部署时保留所使用 Qt、OpenCV 及其运行依赖的相应许可文件。
