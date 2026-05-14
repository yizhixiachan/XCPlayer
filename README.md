# XCPlayer
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL_3.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)]()
[![Qt 6](https://img.shields.io/badge/Qt-6.8-green)]()
[![FFmpeg 7.1](https://img.shields.io/badge/FFmpeg-7.1.1-red)]()

XCPlayer 是一款面向 Windows 桌面的本地音视频播放器，基于 Qt Quick 构建界面，使用 FFmpeg 7.1.1 负责媒体解析、解码与封装处理，并结合 D3D11/WASAPI 提供更贴近原生桌面体验的播放能力。项目同时覆盖音乐管理、视频播放、媒体库整理、歌词与元数据编辑等常用场景。

---

## 🚀 核心功能
*   **多媒体支持**：支持音视频播放、本地媒体库管理、自动识别媒体资源。
*   **硬件解码**：基于 D3D11VA 硬件加速，支持软/硬解切换。
*   **HDR 显示支持**：支持识别 HDR10/10+、Dolby Vision 及 HLG 等格式；支持在兼容的硬件环境上通过 Direct3D 11 触发系统 HDR 显示模式。
*   **专业音频**：基于 WASAPI 输出，支持 WASAPI 独占 和 ReplayGain 回放增益。
*   **字幕与歌词**：支持多种外挂字幕格式，支持 LRC 歌词同步及桌面歌词功能。
*   **元数据处理**：支持媒体详细信息解析、编辑媒体文本元数据、封面提取与替换。
*   **网络流播放**：支持 HTTP/HTTPS/RTMP 流播放及自定义 Headers。

---

## 🛠 编译指南
本项目自带了 Windows 平台的 FFmpeg 开发库（Include & Lib）。
1. 安装 **Qt 6.9** 或更高版本。
2. 使用 Qt Creator 打开 `CMakeLists.txt`。
3. 配置构建工具链并执行构建。

---

## ⚙ 功能介绍

### 媒体导入与资料库
- 支持批量导入音频、视频文件，也可以扫描整个文件夹并自动识别媒体资源。
- 内置媒体库统计，可查看音乐、视频、艺术家、专辑、已看完视频和 HDR 视频数量。
- 支持按艺术家、专辑、播放列表等维度组织媒体。
- 支持自定义播放列表，可创建、重命名、删除列表，并将媒体加入指定列表。
- 支持拖拽导入文件或文件夹，适合快速整理本地媒体。

### 播放控制
- 支持播放、暂停、上一首、下一首、进度跳转、音量调节和静音。
- 支持列表循环、随机播放、单曲循环等播放模式。
- 支持倍速播放，并在播放过程中动态切换。
- 支持章节信息展示，视频或音频包含章节时可在进度条上快速定位。
- 支持记录视频播放进度，方便继续观看。

### 音视频解码与渲染
- 基于 FFmpeg 实现音频、视频、字幕的解复用与解码。
- 支持 D3D11VA 硬件加速解码，并可在播放时切换软解/硬解。
- 使用 D3D11 VideoProcessor 渲染视频画面，支持亮度、对比度、色相、饱和度、降噪、边缘增强等图像调节。
- 支持 HDR 视频识别与 HDR 开关，包含 HDR10、HDR10+、Dolby Vision、HLG 等格式判断。
- 支持视频旋转、字幕显示以及多音轨、多视频流、多字幕流切换。

### 音频输出
- 基于 WASAPI 输出音频。
- 支持 WASAPI 独占模式，减少系统混音干扰。
- 支持 ReplayGain 回放增益，用于自动平衡不同音频文件的响度。

### 字幕与歌词
- 支持内封字幕流切换。
- 支持加载外挂字幕文件，兼容 `.srt`、`.vtt`、`.ass`。
- 支持 LRC 歌词批量导入，并自动匹配媒体库中的音乐。
- 支持桌面歌词显示，播放时可同步展示当前歌词，滚轮可调整字体大小。
- 支持歌词编辑与时间轴偏移，便于修正歌词同步。

### 媒体信息与元数据
- 支持解析媒体容器、音频流、视频流、字幕流、章节、码率、分辨率、帧率、色彩空间、HDR 信息等详细数据。
- 支持查看并修改标题、艺术家、专辑、歌词等元数据。
- 支持替换、保存媒体封面。
- 支持从封面或本地图片提取主色，用于界面背景和视觉联动。

### 界面与使用体验
- 提供媒体库、播放列表、艺术家/专辑、工具、设置、网络流等多个页面。
- 支持自定义应用背景颜色或背景图片。
- 支持悬浮小窗口播放，滚轮可快速调节音量。
- 播放音乐时可展开封面展示，播放视频时可隐藏控制栏，获得更沉浸的观看体验。

### 网络流播放（Beta）
- 支持播放 HTTP/HTTPS、RTMP 等网络流地址。
- HTTP 流支持自定义 User-Agent 和请求 Headers，便于测试需要特定请求头的媒体源。

---

## 🖼 软件预览
<div align="center">
  <img src="images/p1.png?raw=true" width="45%"> <img src="images/p2.png?raw=true" width="45%">
  <img src="images/p3.png?raw=true" width="45%"> <img src="images/p4.png?raw=true" width="45%">
  <img src="images/p5.png?raw=true" width="45%"> <img src="images/p6.png?raw=true" width="45%">
  <img src="images/p7.png?raw=true" width="45%"> <img src="images/p8.png?raw=true" width="45%">
</div>

---

## ⚖️ 许可与声明
本项目采用 **[GNU General Public License v3.0 (GPL-3.0)](LICENSE)** 开源协议。

**致谢：**
本项目使用了以下优秀的开源框架与库：
*   **[Qt 6](https://www.qt.io/)**: (LGPLv3/GPLv3) 
*   **[FFmpeg 7.1.1](https://ffmpeg.org/)**: (LGPL/GPL) 

---

## ☕ 赞助与支持
本项目为开源项目，作者利用业余时间开发。如果你觉得 XCPlayer 对你有帮助，欢迎支持作者！

<div align="center">
  <img src="assets/pay.jpg?raw=true" width="280" alt="赞助二维码">
  <p>微信 / 支付宝扫码赞助</p>
</div>

---
*如果您在使用过程中发现任何问题，欢迎提交 [Issues](https://github.com/yizhixiachan/XCPlayer/issues)。*
