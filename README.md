<div align="center">

# 🎧 PodRadio

**Terminal Podcast/Radio Player**

*A powerful ncurses-based media player for the command line*

[![Version](https://img.shields.io/badge/version-v0.05--B9n3e5g3n-blue.svg)](https://github.com/deadship2003/Podradio)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](https://github.com/deadship2003/Podradio)
[![C++](https://img.shields.io/badge/C++-17-orange.svg)](https://isocpp.org/)
[![Lines](https://img.shields.io/badge/lines-12%2C934-informational.svg)](https://github.com/deadship2003/Podradio)

**[Features](#-功能特性) • [Install](#-安装) • [Usage](#-使用方法) • [Build](#-编译选项)**

<img src="https://img.shields.io/badge/TuneIn-📻-red?style=flat-square" alt="TuneIn"/> 
<img src="https://img.shields.io/badge/Podcast-🎙️-purple?style=flat-square" alt="Podcast"/> 
<img src="https://img.shields.io/badge/YouTube-📺-red?style=flat-square" alt="YouTube"/> 
<img src="https://img.shields.io/badge/Local-🎵-blue?style=flat-square" alt="Local"/>

</div>

---

## 🎵 功能特性

### 媒体支持
- **📻 电台流媒体** - TuneIn电台目录完整支持
- **🎙️ 播客订阅** - RSS/Atom播客订阅与播放
- **📺 YouTube** - YouTube频道、视频、播放列表
- **🎵 本地文件** - MP3, FLAC, OGG, M4A, WAV, MP4, MKV, WebM

### 播放控制
- **终端TUI界面** - 美观的ncurses交互界面
- **播放控制** - 播放/暂停、音量、速度调节
- **进度追踪** - 自动保存播放位置，断点续播
- **播放列表** - 创建和管理播放列表
- **下载管理** - 节目下载到本地

### 数据管理
- **收藏夹** - 收藏喜欢的电台和播客
- **播放历史** - 完整的播放历史记录
- **搜索功能** - 全局内容搜索
- **OPML导入导出** - 订阅备份与恢复

---

## 📥 安装

### Linux (Arch Linux) ⭐ 推荐

```bash
# 安装依赖
sudo pacman -S --needed mpv ncurses curl libxml2 sqlite fmt nlohmann-json cmake ninja gcc

# 克隆并编译
git clone https://github.com/podradio/podradio.git
cd podradio
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
sudo cmake --install build

# 运行
podradio
```

### Linux (Debian/Ubuntu)

```bash
# 安装依赖
sudo apt-get update
sudo apt-get install -y mpv libmpv-dev libncurses5-dev libncursesw5-dev \
    libcurl4-openssl-dev libsqlite3-dev libxml2-dev libfmt-dev \
    nlohmann-json3-dev cmake ninja-build g++

# 编译
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
sudo cmake --install build
```

### Linux (Fedora)

```bash
# 安装依赖
sudo dnf install -y mpv mpv-devel ncurses-devel libcurl-devel sqlite-devel \
    libxml2-devel fmt-devel nlohmann-json-devel cmake ninja-build gcc-c++

# 编译
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
sudo cmake --install build
```

### macOS

```bash
# 安装依赖 (Homebrew)
brew install mpv ncurses curl sqlite libxml2 fmt nlohmann-json cmake ninja

# 编译
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(sysctl -n hw.ncpu)
sudo cmake --install build
```

### Windows (vcpkg + MSVC)

```powershell
# 安装vcpkg
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

# 编译
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
```

---

## 🔨 交叉编译

### Linux ARM64 (aarch64) - 从 x86_64 交叉编译

```bash
# 1. 安装交叉编译工具链
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# 2. 添加 ARM64 架构支持
sudo dpkg --add-architecture arm64
sudo sed -i 's/deb http/deb [arch=amd64] http/' /etc/apt/sources.list

# 3. 添加 ARM64 软件源
sudo bash -c 'cat > /etc/apt/sources.list.d/arm64.list << EOF
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-updates main restricted universe multiverse
EOF'

# 4. 更新并安装 ARM64 依赖
sudo apt-get update
sudo apt-get install -y \
    libncurses5-dev:arm64 libncursesw5-dev:arm64 \
    libcurl4-openssl-dev:arm64 libsqlite3-dev:arm64 \
    libxml2-dev:arm64 libfmt-dev:arm64 \
    nlohmann-json3-dev:arm64 cmake ninja-build

# 5. 尝试安装 MPV ARM64 (可选)
sudo apt-get install -y libmpv-dev:arm64 || echo "MPV ARM64 需要手动编译"

# 6. 配置并编译
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux.cmake
cmake --build build --parallel $(nproc)

# 7. 验证生成的二进制文件
file build/podradio
# 输出应为: build/podradio: ELF 64-bit LSB executable, ARM aarch64...
```

### Windows MinGW - 从 Linux 交叉编译

```bash
# 1. 安装 MinGW-w64
sudo apt-get install -y mingw-w64

# 2. 安装 Windows 依赖 (通过 vcpkg 或手动下载)
# 方式A: 使用 vcpkg 交叉编译依赖
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install curl sqlite3 libxml2 fmt nlohmann-json --triplet=x64-mingw-static

# 3. 配置并编译
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-windows-mingw.cmake \
    -DCMAKE_PREFIX_PATH=/path/to/vcpkg/installed/x64-mingw-static
cmake --build build --parallel $(nproc)

# 4. 验证生成的二进制文件
file build/podradio.exe
# 输出应为: build/podradio.exe: PE32+ executable (console) x86-64...

# 注意: 运行时需要 mpv.dll 和其他 DLL 文件
```

### macOS Universal Binary - 同时支持 Intel 和 M1/M2

```bash
# 1. 确保安装了 Xcode 命令行工具
xcode-select --install

# 2. 安装通用库 (需要手动编译或使用 Homebrew)
# Homebrew 不直接支持通用二进制，需要手动处理

# 3. 配置并编译
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-macos-universal.cmake
cmake --build build --parallel $(sysctl -n hw.ncpu)

# 4. 验证生成的二进制文件
lipo -archs build/podradio
# 输出应为: x86_64 arm64
```

---

## 📋 编译选项

### CMake 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `CMAKE_BUILD_TYPE` | `Release` | 构建类型: Debug/Release |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | 安装路径 |
| `CMAKE_TOOLCHAIN_FILE` | - | 交叉编译工具链文件 |

### 常用编译命令

```bash
# Debug 构建 (包含调试符号)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel $(nproc)

# Release 构建 (优化编译)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)

# 指定安装路径
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel $(nproc)
sudo cmake --install build

# 清理构建目录
rm -rf build && cmake -B build -G Ninja
```

### CMake 预设

```bash
# Linux Release
cmake --preset linux-release
cmake --build --preset linux-release

# macOS Release
cmake --preset macos-release
cmake --build --preset macos-release

# Windows Release
cmake --preset windows-release
cmake --build --preset windows-release
```

---

## 🚀 使用方法

### 命令行参数

```bash
podradio                    # 启动TUI界面
podradio -a <url>           # 添加订阅源
podradio -i <file>          # 导入OPML
podradio -e <file>          # 导出OPML
podradio -t <time>          # 睡眠定时器 (格式: 5h/30m/1:25:15)
podradio --purge            # 清除缓存
podradio -h                 # 显示帮助
podradio -v                 # 显示版本
```

### 快捷键

| 按键 | 功能 |
|------|------|
| `j`/`↓` | 向下移动 |
| `k`/`↑` | 向上移动 |
| `l`/`Enter` | 播放/展开 |
| `h` | 返回/折叠 |
| `Space`/`p` | 暂停/继续 |
| `+`/`-` | 音量增减 |
| `[`/`]` | 速度增减 |
| `a` | 添加订阅 |
| `d` | 删除 |
| `f` | 收藏 |
| `/` | 搜索 |
| `?` | 帮助 |
| `q` | 退出 |

### 模式切换

| 按键 | 模式 |
|------|------|
| `R` | 电台模式 |
| `P` | 播客模式 |
| `F` | 收藏夹 |
| `H` | 历史记录 |
| `O` | 在线搜索 |
| `M` | 循环切换模式 |

---

## 📁 项目结构

```
PodRadio/
├── CMakeLists.txt              # CMake 配置
├── CMakePresets.json           # CMake 预设
├── vcpkg.json                  # vcpkg 依赖管理
├── src/
│   └── podradio.cpp            # 主程序源码 (12,934 行)
├── man/
│   └── podradio.1              # man 手册页
├── cmake/
│   ├── toolchain-aarch64-linux.cmake  # Linux ARM64 交叉编译
│   ├── toolchain-windows-mingw.cmake  # Windows MinGW 交叉编译
│   └── toolchain-macos-universal.cmake # macOS 通用二进制
├── .github/
│   └── workflows/
│       └── build.yml           # GitHub Actions CI/CD
├── README.md                   # 项目说明
├── LICENSE                     # MIT 许可证
└── build.sh                    # 构建脚本
```

---

## 📋 依赖库

| 库 | 版本要求 | 用途 | Arch 包名 | Debian 包名 |
|---|---------|------|-----------|-------------|
| libmpv | 0.34+ | 媒体播放 | `mpv` | `libmpv-dev` |
| ncurses | 6.0+ | 终端UI | `ncurses` | `libncurses5-dev` |
| libcurl | 7.80+ | 网络请求 | `curl` | `libcurl4-openssl-dev` |
| libxml2 | 2.9+ | XML/RSS解析 | `libxml2` | `libxml2-dev` |
| SQLite3 | 3.40+ | 数据存储 | `sqlite` | `libsqlite3-dev` |
| fmt | 10.0+ | 格式化输出 | `fmt` | `libfmt-dev` |
| nlohmann_json | 3.11+ | JSON处理 | `nlohmann-json` | `nlohmann-json3-dev` |
| yt-dlp | 可选 | YouTube支持 | `yt-dlp` | `yt-dlp` |

---

## 📂 数据存储

所有用户数据存储在 `~/.local/share/podradio/`:

| 文件/目录 | 说明 |
|----------|------|
| `podradio.db` | SQLite数据库 (订阅、历史、缓存) |
| `downloads/` | 下载的媒体文件 |
| `cache/` | 缓存的媒体和元数据 |

配置文件位置: `~/.config/podradio/podradio.ini`

---

## 📄 许可证

MIT License - 详见 [LICENSE](LICENSE)

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

---

## 📮 联系方式

- **作者**: Panic
- **邮箱**: Deadship2003@gmail.com
- **Issues**: [GitHub Issues](https://github.com/podradio/podradio/issues)

---

<p align="center">
  Made with ❤️ by <a href="mailto:Deadship2003@gmail.com">Panic</a>
</p>
