# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                              P O D R A D I O                              ║
# ║                   Terminal Podcast/Radio Player                           ║
# ║                 Author: Panic <Deadship2003@gmail.com>                    ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

<p align="center">
  <strong>V0.05B9n3e5g3n</strong> | <strong>12,900+ 行代码</strong> | <strong>完整功能</strong>
</p>

<p align="center">
  <a href="#功能特性">功能特性</a> •
  <a href="#安装">安装</a> •
  <a href="#使用方法">使用方法</a> •
  <a href="#快捷键">快捷键</a> •
  <a href="#配置">配置</a> •
  <a href="#编译">编译</a>
</p>

---

## 🎵 功能特性

### 媒体支持
| 类型 | 描述 |
|------|------|
| 📻 **电台流媒体** | TuneIn电台目录完整支持，全球数千个电台 |
| 🎙️ **播客订阅** | RSS/Atom播客订阅与播放，自动更新 |
| 📺 **YouTube** | YouTube频道、视频、播放列表支持 |
| 🎵 **本地文件** | MP3, FLAC, OGG, M4A, WAV, MP4, MKV, WebM |

### 播放控制
- **终端TUI界面** - 美观的ncurses交互界面，双面板布局
- **播放控制** - 播放/暂停、音量(0-150%)、速度(0.25x-4.0x)
- **进度追踪** - 自动保存播放位置，断点续播
- **播放列表** - 创建和管理播放列表，支持循环/随机播放
- **下载管理** - 节目下载到本地，离线播放

### 数据管理
- **收藏夹** - 收藏喜欢的电台和播客
- **播放历史** - 完整的播放历史记录
- **搜索功能** - 全局内容搜索，支持正则表达式
- **OPML导入导出** - 订阅备份与恢复

### 高级功能
- **睡眠定时器** - 设置自动停止播放时间
- **多选操作** - Visual模式批量标记和操作
- **图标风格** - ASCII/Emoji图标切换
- **自定义配色** - 状态栏彩虹效果和自定义颜色

---

## 📥 安装

### Linux (Arch Linux)

```bash
# 安装依赖
sudo pacman -S mpv ncurses curl libxml2 sqlite fmt nlohmann-json cmake ninja gcc

# 克隆并编译
git clone https://github.com/YOUR_USERNAME/PodRadio.git
cd PodRadio
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

### Linux (Debian/Ubuntu)

```bash
# 安装依赖
sudo apt-get install mpv libmpv-dev libncurses5-dev libncursesw5-dev \
    libcurl4-openssl-dev libsqlite3-dev libxml2-dev libfmt-dev \
    nlohmann-json3-dev cmake ninja-build g++

# 编译
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

### Linux (Fedora)

```bash
# 安装依赖
sudo dnf install mpv mpv-devel ncurses-devel libcurl-devel sqlite-devel \
    libxml2-devel fmt-devel nlohmann-json-devel cmake ninja-build gcc-c++

# 编译
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

### macOS

```bash
# 安装依赖 (Homebrew)
brew install mpv ncurses curl sqlite libxml2 fmt nlohmann-json cmake ninja

# 编译
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

### Windows (MSYS2/MinGW)

```bash
# 安装依赖
pacman -S mingw-w64-x86_64-mpv mingw-w64-x86_64-ncurses \
    mingw-w64-x86_64-curl mingw-w64-x86_64-sqlite3 \
    mingw-w64-x86_64-libxml2 mingw-w64-x86_64-fmt \
    mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc

# 编译
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

---

## 🚀 使用方法

### 命令行参数

```bash
podradio                    # 启动TUI界面
podradio -a <url>           # 添加订阅源
podradio -i <file>          # 导入OPML文件
podradio -e <file>          # 导出OPML文件
podradio -t <time>          # 睡眠定时器
podradio --purge            # 清除所有缓存数据
podradio -h                 # 显示帮助
podradio -v                 # 显示版本
```

### 时间格式示例
```bash
podradio -t 30m             # 30分钟后停止
podradio -t 2h              # 2小时后停止
podradio -t 1:30:00         # 1小时30分钟后停止
```

---

## ⌨️ 快捷键

### 导航

| 按键 | 功能 |
|------|------|
| `j` / `↓` | 向下移动 |
| `k` / `↑` | 向上移动 |
| `g` | 跳到第一项 |
| `G` | 跳到最后一项 |
| `Ctrl+U` | 向上翻半页 |
| `Ctrl+D` | 向下翻半页 |
| `h` | 折叠文件夹 / 返回上一级 |
| `l` / `Enter` | 展开文件夹 / 播放选中项 |

### 播放控制

| 按键 | 功能 |
|------|------|
| `Space` / `p` | 暂停 / 继续 |
| `+` | 音量增加 (步长5%) |
| `-` | 音量减少 (步长5%) |
| `[` | 减慢播放速度 |
| `]` | 加快播放速度 |
| `\` | 重置播放速度为 1.0x |

### 模式切换

| 按键 | 模式 |
|------|------|
| `R` | 电台模式 (Radio) |
| `P` | 播客模式 (Podcast) |
| `F` | 收藏夹 (Favorites) |
| `H` | 历史记录 (History) |
| `O` | 在线搜索 (Online) |
| `M` | 循环切换模式 |
| `B` | 切换地区 (Online模式) |

### 播放列表 (List模式)

| 按键 | 功能 |
|------|------|
| `L` | 进入/退出 List 模式 |
| `=` | 将选中项添加到播放列表 |
| `o` | 切换排序顺序 (A-Z / Z-A) |
| `J` | 列表中下移选中项 |
| `K` | 列表中上移选中项 |
| `C` | 清空播放列表 |

### 标记与Visual模式

| 按键 | 功能 |
|------|------|
| `m` | 标记/取消标记当前项 |
| `v` | 进入 Visual 选择模式 |
| `V` | 取消 Visual 模式 / 清除所有标记 |
| `d` | 删除标记项 |
| `D` | 下载标记项 |

### 其他操作

| 按键 | 功能 |
|------|------|
| `a` | 添加新订阅 |
| `e` | 编辑当前项 (标题/URL) |
| `f` | 添加到收藏夹 |
| `r` | 刷新当前视图 |
| `/` | 开始搜索 |
| `n` | 下一个搜索结果 |
| `N` | 上一个搜索结果 |
| `?` | 显示帮助对话框 |
| `U` | 切换图标风格 (ASCII/Emoji) |
| `S` | 切换滚动模式 |
| `T` | 切换树形连接线 |
| `q` | 退出程序 |

---

## ⚙️ 配置

配置文件位置: `~/.config/podradio/podradio.ini`

### 配置示例

```ini
[general]
auto_resume = true
default_volume = 100
default_speed = 1.0
play_mode = single

[download]
download_dir = ~/Music/PodRadio
max_concurrent = 3

[history]
history_max_records = 2048
history_max_days = 1080

[youtube]
video_max_height = 1080
video_max_fps = 30
download_mode = merged
audio_format = mp3
audio_quality = 0

[statusbar_color]
mode = rainbow
update_interval_ms = 100
rainbow_speed = 1
```

### 播放模式
- `single` - 单曲播放，播放完毕后停止
- `cycle` - 列表循环，播放完毕后从头开始
- `random` - 随机播放

---

## 🔨 编译

### CMake 配置选项

```bash
# Debug 构建 (包含调试符号)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Release 构建 (优化编译)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 指定安装路径
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local

# 使用 vcpkg 工具链
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
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

### 交叉编译

```bash
# ARM64 Linux
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux.cmake
cmake --build build

# Windows MinGW
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-windows-mingw.cmake
cmake --build build
```

---

## 📁 项目结构

```
PodRadio/
├── CMakeLists.txt              # CMake 配置
├── CMakePresets.json           # CMake 预设
├── vcpkg.json                  # vcpkg 依赖管理
├── src/
│   └── podradio.cpp            # 主程序源码 (12,900+ 行)
├── man/
│   └── podradio.1              # man 手册页
├── cmake/
│   ├── toolchain-aarch64-linux.cmake  # ARM64 工具链
│   └── toolchain-windows-mingw.cmake  # Windows MinGW 工具链
├── .github/
│   └── workflows/
│       └── build.yml           # GitHub Actions CI/CD
├── README.md                   # 项目说明
├── LICENSE                     # MIT 许可证
└── build.sh                    # 构建脚本
```

---

## 📋 依赖库

| 库 | 版本要求 | 用途 | 包名 (Arch) |
|---|---------|------|-------------|
| libmpv | 0.34+ | 媒体播放后端 | `mpv` |
| ncurses | 6.0+ | 终端UI | `ncurses` |
| libcurl | 7.80+ | 网络请求 | `curl` |
| libxml2 | 2.9+ | XML/RSS解析 | `libxml2` |
| SQLite3 | 3.40+ | 数据存储 | `sqlite` |
| fmt | 10.0+ | 格式化输出 | `fmt` |
| nlohmann_json | 3.11+ | JSON处理 | `nlohmann-json` |
| yt-dlp | 可选 | YouTube支持 | `yt-dlp` |

---

## 🏗️ 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                     表现层 (UI Layer)                        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   Terminal UI   │  │   Input Handler │  │  Renderer   │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                      业务层 (Business Layer)                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │  PodRadioApp    │  │    TreeNode     │  │  Playlist   │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                       数据层 (Data Layer)                    │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │ DatabaseManager │  │  CacheManager   │  │OPML Handler │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                      服务层 (Service Layer)                  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │ MPVController   │  │    Network      │  │ RSS Parser  │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                       工具层 (Utility Layer)                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │     Utils       │  │     Logger      │  │   Config    │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## 📂 数据存储

所有用户数据存储在 `~/.local/share/podradio/`:

| 文件/目录 | 说明 |
|----------|------|
| `podradio.db` | SQLite数据库 (订阅、历史、缓存) |
| `downloads/` | 下载的媒体文件 |
| `cache/` | 缓存的媒体和元数据 |

---

## 📄 许可证

MIT License - 详见 [LICENSE](LICENSE)

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

### 开发环境设置

```bash
# 克隆仓库
git clone https://github.com/YOUR_USERNAME/PodRadio.git
cd PodRadio

# 创建开发分支
git checkout -b feature/your-feature

# 编译调试版本
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 运行测试
./build/podradio
```

---

## 📮 联系方式

- **作者**: Panic
- **邮箱**: Deadship2003@gmail.com
- **Issues**: [GitHub Issues](https://github.com/YOUR_USERNAME/PodRadio/issues)

---

<p align="center">
  Made with ❤️ by <a href="mailto:Deadship2003@gmail.com">Panic</a>
</p>
