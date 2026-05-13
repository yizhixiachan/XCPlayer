# XCPlayer
基于 Qt6 和 FFmpeg 7.1.1 开发的本地播放器。

## 系统要求

  **操作系统**：**仅支持 Windows 10 及以上系统 (64-bit)**。

## 编译指南 (Windows)

  **本项目自带了 Windows 平台的 FFmpeg 7.1.1 开发库（Include & Lib）**。

  1. 安装 Qt 6.8 或更高版本。
  2. 使用 Qt Creator 打开 `CMakeLists.txt`。
  3. 直接点击构建（Build）即可。

## 主要功能
  支持自定义播放列表、自定义应用背景。
  支持流切换、倍速切换、软硬解切换。
  支持媒体信息解析、元数据修改、歌词导入、字幕导入。
  支持WASAPI 独占模式。
  支持硬件加速解码（D3D11VA）。
