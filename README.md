# XCPlayer
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL_3.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)]()
[![Qt 6](https://img.shields.io/badge/Qt-6.8-green)]()
[![FFmpeg 7.1](https://img.shields.io/badge/FFmpeg-7.1.1-red)]()

XCPlayer 是一款基于 **Qt 6.9** 和 **FFmpeg 7.1.1** 开发的高性能 Windows 本地播放器，旨在提供丝滑的原生桌面体验。

---

## 🚀 核心功能
*   **多媒体支持**：支持音视频播放、本地媒体库管理、自动识别媒体资源。
*   **硬件解码**：基于 D3D11VA 硬件加速，支持软/硬解切换。
*   **HDR 显示支持**：支持识别 HDR10/10+、Dolby Vision 及 HLG 等格式；支持在兼容的硬件环境上通过 Direct3D 11 触发系统 HDR 显示模式。
*   **专业音频**：基于 WASAPI 输出，支持 WASAPI 独占 和ReplayGain 回放增益。
*   **字幕与歌词**：支持多种外挂字幕格式，支持 LRC 歌词同步及桌面歌词功能。
*   **元数据处理**：支持媒体详细详细信息解析、编辑媒体文本元数据、封面提取与替换。
*   **网络流播放**：支持 HTTP/HTTPS/RTMP 流播放及自定义 Headers。

---

## 🛠 编译指南
本项目自带了 Windows 平台的 FFmpeg 开发库（Include & Lib）。
1. 安装 **Qt 6.9** 或更高版本。
2. 使用 Qt Creator 打开 `CMakeLists.txt`。
3. 配置构建工具链并执行构建。

---

## ⚖️ 许可与声明
本项目采用 **[GNU General Public License v3.0 (GPL-3.0)](LICENSE)** 开源协议。

**致谢：**
本项目使用了以下优秀的开源框架与库：
*   **[Qt 6](https://www.qt.io/)**: (LGPLv3/GPLv3) - 构建高性能桌面 UI。
*   **[FFmpeg 7.1.1](https://ffmpeg.org/)**: (LGPL/GPL) - 提供强大的音视频处理能力。

---

## ☕ 赞助与支持
本项目为开源项目，作者利用业余时间开发。如果你觉得 XCPlayer 对你有帮助，欢迎请作者喝瓶水！你的支持是我持续维护和优化的动力。

<div align="center">
  <img src="https://github.com/yizhixiachan/XCPlayer/blob/main/assets/donate.png?raw=true" width="280" alt="赞助二维码">
  <p>微信 / 支付宝扫码赞助</p>
</div>

---
*如果您在使用过程中发现任何问题或有改进建议，欢迎提交 [Issues](https://github.com/yizhixiachan/XCPlayer/issues)。*
