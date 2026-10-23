# RM2027 BaseVision · 基地视野

**基地掉血后，图传究竟会糊到什么程度？**

面向 **RoboMaster 2027 赛季新规则测试**的实时摄像头画质模拟器：拖动己方基地血量，观察后勤系数如何影响图传清晰度。适合操作手训练、细节辨识测试，以及视觉算法预研时的画质观察。

[![Build and test](https://github.com/mc260/robomaster-2027-vision-simulator/actions/workflows/ci.yml/badge.svg)](https://github.com/mc260/robomaster-2027-vision-simulator/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)
![Qt6](https://img.shields.io/badge/Qt-6-41CD52.svg)
![OpenCV4](https://img.shields.io/badge/OpenCV-4-5C3EE8.svg)

[English](README.en.md) · [构建与使用](docs/BUILDING.md) · [规则出处与算法](docs/RULES.md) · [验证记录](VALIDATION.md) · [反馈问题](https://github.com/mc260/robomaster-2027-vision-simulator/issues)

![BaseVision 桌面程序，使用合成规则图演示](docs/assets/desktop.png)

*截图使用合成测试图，不是真实摄像头画面。*

## 功能

- **血量驱动画质**：0～10000，滑块与数值输入同步，实时生效。
- **按规则处理**：面积平均降采样，再双线性放大回 1920×1080。
- **同视野原图对照**：一键切换降质前的基准画面。
- **1:1 像素检查**：一图像像素对应一屏幕物理像素，支持高 DPI；大图可滚动查看。
- **真实帧率显示**：目标 1～240 FPS，默认 60；显示实际预览 FPS，重复重绘旧帧不增加计数。
- **实时摄像头**：设备选择、自动模式回退、完整视野补黑边，以及只保留最新帧的处理链路。
- **海康工业相机**：通过已安装的 MVS SDK 接入 USB 工业相机，与普通摄像头共用血量降质和原图对照；已实测 MV-CS060-10UC-PRO。
- **C++ 桌面程序**：Qt 6 Widgets + OpenCV；Linux 已进行真机验证，Windows 提供源码与构建部署脚本。

## 一眼看懂规则效果

同一张 6 px、3 px、1 px 棋盘格，使用程序实际的处理函数生成。下图每块均为**未经再次缩放的 1:1 裁切**：

![满血、半血和零血量的处理结果](docs/assets/rule-comparison.png)

| 基地血量 H | 后勤系数 λ | 中间采样分辨率 | 放大后的画布 |
|---:|---:|---:|---:|
| 10000 | 1.00 | 1920×1080 | 1920×1080 |
| 5000 | 0.75 | 1440×810 | 1920×1080 |
| 0 | 0.50 | 960×540 | 1920×1080 |

```text
λ = 1 − (10000 − H) / 20000
中间宽高 = round(1920λ) × round(1080λ)

摄像头帧
  → 等比例适配并补黑边至 1920×1080
  → INTER_AREA 降采样
  → INTER_LINEAR 放大回 1920×1080
  → 自适应预览 / 1:1 像素检查
```

**λ² 缩放的是总像素数，宽、高分别乘 λ。** 满血时不额外降质；零血量时仍为半宽、半高，不会黑屏。

依据官方 **《RoboMaster 2027 机甲大师超级对抗赛比赛规则手册 V1.0.0（20261015）》5.6.1、表 5-19、图 5-16**。见[官方规则页面](https://bbs.robomaster.com/wiki/20204847/1969149)与[规则说明](docs/RULES.md)。本项目为社区测试工具，后续规则修订需要同步适配。

## 快速开始：Linux

```bash
sudo apt install git build-essential cmake qt6-base-dev libopencv-dev
git clone https://github.com/mc260/robomaster-2027-vision-simulator.git
cd robomaster-2027-vision-simulator
bash scripts/build-linux.sh
bash scripts/run-linux.sh
```

1. 选择摄像头，点击 **启动摄像头**。
2. 把血量依次设为 **10000、5000、0**，观察变化。
3. 点击 **1:1 像素检查**，并切换 **查看原始画面**，比较细节。
4. 调整设备或 FPS 前先停止摄像头。

海康 USB 工业相机需安装 **MVS 4.7 或更新版本的开发包和运行库**后构建，在设备列表中选择“海康 MVS”。工业相机保留完整传感器视野，再适配至 1080p；USB 2.0 连接可能限制帧率。详见[海康接入说明](docs/BUILDING.md#海康-usb-工业相机mvs)。

需要 CMake ≥3.21、C++17 编译器、Qt ≥6.2、OpenCV 4.x。完整说明见[构建文档](docs/BUILDING.md)。

## Windows

提供 MSVC x64 + Qt 6 + OpenCV 的[构建与部署步骤](docs/BUILDING.md#windows-构建)及 PowerShell 脚本。**当前 Windows 源码与脚本尚未在 Windows 实机验证**；欢迎提交构建结果和摄像头兼容性反馈。

## 为什么预览看起来变化不大？

自适应预览会把 1920×1080 再缩小到窗口，可能掩盖高频细节差异。请用 **1:1 像素检查**对照文字、细线和棋盘格，同时确认正在查看“处理后画面”。低清摄像头放大到 1080p 也无法恢复原本没有的细节。

1920×1080 是放大后的画布，真正的采样尺寸显示在“降采样分辨率”中。目标 60 FPS 是请求值，实际速度取决于摄像头和电脑。

## 可复核，而不只靠肉眼

```bash
ctest --test-dir build --output-on-failure
./build/rule_demo build/rule-demo
# 可选：需要 Pillow，用于生成文档对照图
python3 tools/make_comparison.py build/rule-demo docs/assets/rule-comparison.png
```

测试覆盖公式、取整、黑边、棋盘格、独立面积平均/双线性参考公式，以及 **100% / 200% DPI 下逐像素一致的 1:1 显示**。普通测试不打开摄像头，真机测试步骤见[验证记录](VALIDATION.md)。

## 参与与支持

如果它帮助你评估 27 赛季图传效果，欢迎 **Star**，让更多 RM 队伍找到这个工具。

欢迎提交规则修订、Linux/Windows 构建结果、摄像头兼容性 Issue，以及改进测试和界面的 PR。请参阅[贡献说明](CONTRIBUTING.md)。

## 许可证

源码采用 [MIT](LICENSE) 许可证。Qt、OpenCV 及附带依赖遵循各自许可证。规则文件与 RoboMaster 品牌归各自权利人所有；本仓库不分发官方规则 PDF，也不代表组委会。
