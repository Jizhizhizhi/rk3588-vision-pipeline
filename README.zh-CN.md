# RK3588 视觉处理链路

[English](README.md) | **简体中文**

这是一个仅包含后端的 Linux 参考工程，涵盖 V4L2 视频采集、最新结果优先的
推理调度、可插拔的目标控制链路，以及 DRM/KMS HDMI 输出。仓库不依赖任何
Web 前端、授权服务、模型文件、私有设备配置或系统镜像。

> **负责任使用声明：** 项目作者明确禁止使用本项目制作、运行、传播或协助
> 游戏作弊、外挂、破坏公平竞技的自动化工具，以及未经授权的竞技辅助工具。
> 任何 Fork、复制、重新打包、改名或第三方衍生项目，除非作者另行书面确认，
> 均不代表作者参与、授权、认可或背书。完整内容见
> [免责声明与使用声明](DISCLAIMER.md)。

## 已开源内容

- 带有轻量 RAII 封装的 V4L2 MMAP 视频采集。
- 容量受限、始终保留最新帧的推理输入队列。
- 多工作线程推理调度，只把最新完成的结果交给下游。
- 目标控制策略接口和分发链路，不包含目标选择、跟踪、PID、预测或鼠标控制算法。
- 使用 XRGB8888 Dumb Buffer 的 DRM/KMS HDMI 输出。
- EDID 高层设计说明，不包含平台专用应用源码、设备探测逻辑或采集设备信息。

## 明确未开源内容

- Web 前端与 HTTP API。
- 目标选择和运动控制算法。
- 模型权重与专有 SDK 二进制文件。
- 激活、云控、遥测、设备指纹或自动更新服务。
- USB 鼠标透传与输入注入。
- 可直接用于生产环境的 EDID 合成和应用源码。

## 构建

Debian 或 Ubuntu：

```bash
sudo apt install build-essential cmake pkg-config libdrm-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

演示程序使用空推理后端。接入 RKNN 时，实现 `vision::InferenceBackend` 即可；
推理调度和下游“最新结果优先”语义不依赖具体模型运行时。

```bash
sudo ./build/vision_pipeline_demo --capture /dev/video0 --drm /dev/dri/card0
```

采集设备需要提供与当前显示模式匹配的 YUYV 视频帧。访问 DRM/KMS 通常需要
root 权限，或者正确配置用户组和 udev 权限。

## 数据链路

```text
V4L2 视频采集
    | 只保留尚未处理的最新帧
    v
InferenceCoordinator（N 个工作线程）
    | 只保留 frame_id 最大的最新完成结果
    +----> ControlDispatcher -> 用户自行实现的策略与输出端
    |
    +----> 显示输出 -> YUYV 转 XRGB8888 -> DRM/KMS HDMI
```

架构说明见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)，EDID 设计思路见
[docs/EDID_DESIGN.md](docs/EDID_DESIGN.md)，RKNN 后端接入边界见
[docs/RKNN_BACKEND.md](docs/RKNN_BACKEND.md)。

## 负责任使用

本项目是面向本地、已授权视觉实验的系统编程参考工程。使用者应自行遵守适用
法律、平台规则、设备保修条件及第三方服务条款。

作者不授权将本项目用于作弊、外挂、破坏公平竞技的自动化工具，或绕过平台保护
机制。第三方对本项目进行复制、修改、重新分发，或整合进其他项目，均属于第三方
独立行为，不代表作者参与、认可、授权、背书或承担责任。完整内容见
[免责声明与使用声明](DISCLAIMER.md)。

## 许可证

MIT。
