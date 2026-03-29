/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    PODRADIO V0.05B9n3e5g3n                                ║
 * ║                   Terminal Podcast/Radio Player                           ║
 * ║                 Author: Panic <Deadship2003@gmail.com>                    ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【概述】终端播客/电台播放器，支持TuneIn电台、RSS播客、YouTube、本地文件
 * ═════════════════════════════════════════════════════════════════════════════
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【编译命令】
 * ═════════════════════════════════════════════════════════════════════════════
 *   g++ -std=c++17 podradio.cpp -o podradio \
 *       -lmpv -lncurses -lcurl -lxml2 -lfmt -lpthread -lsqlite3
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【命令行参数】
 * ═════════════════════════════════════════════════════════════════════════════
 *   podradio                    启动TUI界面
 *   podradio -a <url>           添加订阅源
 *   podradio -i <file>          导入OPML
 *   podradio -e <file>          导出OPML
 *   podradio -t <time>          睡眠定时器 (格式: 5h/30m/1:25:15)
 *   podradio --purge            清除缓存
 *   podradio -h                 显示帮助
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【热键速查】
 * ═════════════════════════════════════════════════════════════════════════════
 *   导航: k/↑上 j/↓下 l/Enter播放展开 h返回 g顶部 G底部 PageUp/PageDown翻页
 *   模式: R电台 P播客 F收藏 H历史 O在线 M循环切换
 *   播放: Space/p暂停 +/-音量 =加入L列表 [/]变速 r刷新 D下载
 *   管理: a添加 d删除 e编辑 f收藏 m标记 v多选 V清除标记
 *   搜索: /搜索 J下一个 K上一个
 *   界面: T树线 S滚动 L列表弹窗 Ctrl+L主题 ?帮助 q退出
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【架构层次】
 * ═════════════════════════════════════════════════════════════════════════════
 *   表现层: UI类 (终端渲染) / LayoutMetrics (布局管理)
 *   业务层: PodRadioApp (主控) / TreeNode (数据模型)
 *   数据层: DatabaseManager (SQLite) / CacheManager (缓存)
 *   服务层: Network (HTTP) / MPVController (播放)
 *   工具层: Utils (通用) / Logger (日志) / IniConfig (配置)
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【数据存储】
 * ═════════════════════════════════════════════════════════════════════════════
 *   ~/.local/share/podradio/podradio.db  - SQLite数据库
 *   ~/.local/share/podradio/podradio.log - 运行日志
 *   ~/.config/podradio/config.ini        - 配置文件
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【数据库表】
 * ═════════════════════════════════════════════════════════════════════════════
 *   nodes, progress, history, favourites, playlist, search_cache,
 *   podcast_cache, episode_cache, player_state
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【依赖库】
 * ═════════════════════════════════════════════════════════════════════════════
 *   libmpv, ncurses, libcurl, libxml2, nlohmann/json, fmt, sqlite3
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【开发要点】
 * ═════════════════════════════════════════════════════════════════════════════
 *   1. 终端初始化: ioctl(TIOCGWINSZ)获取大小，resizeterm()强制设置
 *   2. 终端退出: atexit注册清理，信号处理确保endwin()调用
 *   3. MPV初始化: 创建时重定向stdout/stderr，设置LC_NUMERIC="C"
 *   4. UTF-8处理: mk_wcwidth()计算宽度，按列滚动，严格截断
 *   5. LINK机制: 收藏使用软链接，source_mode标识来源
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * 【版本历史】
 * ═════════════════════════════════════════════════════════════════════════════
 *   V0.05B9n3e5g3n - o排序扩展(支持L列表排序+联动显示)、PAUSED状态填充siblings、PLAYLIST显示优化(5行+对齐)
 *   V0.05B9n3e5g3l - UI优化：矩形窗口边框闭合修复、播放列表删除冗余选中标记
 *   V0.05B9n3e5g3k - 修复SINGLE模式光标同步：INFO区域播放列表正确显示当前播放项
 *   V0.05B9n3e5g3j - L列表生命周期优化：空列表自动填充、光标同步、播放指示
 *   V0.05B9n3e5g3i - L模式INFO&LOG区域播放列表显示同步修复
 *   V0.05B9n3e5g3e - 终端输出修复：mpv/yt-dlp静默模式，避免花屏
 *   V0.05B9n3e5g3d - F模式播放列表修复 + 多类型缓存解析扩展
 *   V0.05B9n3e5g3c - 双播放列表模式：默认模式（自动截取兄弟节点）+ L模式（手动列表优先）
 *   V0.05B9n3e5g3b - 播放列表自动播放重构：播客节目自动收集兄弟节点、L模式截断播放
 *   V0.05B9n3e5g3a - 订阅持久化修复、F模式缓存优先、订阅图标显示优化
 *   V0.05B9n3e5g3 - 注释整理、代码规范化
 *   V0.05B9n3e5g2RF8A21 - YouTube解析缓存修复
 *   V0.05B9n3e5g2RF8A20 - YouTube解析简化、播放列表自动播放
 *   V0.05B9n3e5g2RF8A19 - 播放列表keep-open修复
 *   V0.05B9n3e5g2RF8A18 - 窗口resize UI修复
 *   V0.05B9n3e5g2RF8A11 - List弹窗重构、播放模式支持
 *   V0.05B9n3e5g2RF8A - 数据库重构、统一LINK机制
 *   V0.05A11 - 智能缓存播放、终端状态恢复
 *
 */


#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <map>
#include <set>
#include <queue>
#include <unistd.h>
#include <fcntl.h>
#include <deque>
#include <chrono>
#include <regex>
#include <getopt.h>
#include <ctime>
#include <cmath>     //用于std::isnan/std::isinf
#include <random>
#include <array>
#include <locale.h>
#include <cstdarg>   //va_list for xml_error_handler
#include <signal.h>
#include <sys/stat.h>
#include <termios.h>  //用于保存/恢复终端属性
#include <sys/ioctl.h> //用于获取终端窗口大小
#include <sys/wait.h>  //用于WIFEXITED/WEXITSTATUS宏

// External Libraries
#include <mpv/client.h>
#include <ncurses.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <sqlite3.h>

namespace podradio
{
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    // =========================================================
    //全局预定义常量 - 动态更新版本信息
    // 包含开发日期/时间，显示在底部艺术横幅作者后面
    // 格式: Mar 3 2026 18:04:15 (月份和日期间单空格)
    // =========================================================
    constexpr const char* APP_NAME    = "PODRADIO";
    constexpr const char* VERSION     = "V0.05B9n3e5g3n";
    constexpr const char* AUTHOR      = "Panic";
    constexpr const char* EMAIL       = "Deadship2003@gmail.com";
    constexpr const char* BUILD_TIME  = "Mar 27 2026 19:45:00";  // 开发日期/时间
    constexpr const char* DATA_DIR    = "/.local/share/podradio";
    constexpr const char* CONFIG_DIR  = "/.config/podradio";
    constexpr const char* DOWNLOAD_DIR = "/Downloads/PodRadio";
    
    // =========================================================
    //配置常量区 - 消除魔数
    // =========================================================
    // 播放器配置常量
    constexpr int MAX_VOLUME = 100;              // 最大音量
    constexpr int MIN_VOLUME = 0;                // 最小音量
    constexpr int VOLUME_STEP = 5;               // 音量调节步进
    constexpr double MIN_SPEED = 0.2;            // 最小播放速度
    constexpr double MAX_SPEED = 3.0;            // 最大播放速度
    constexpr double DEFAULT_SPEED = 1.0;        // 默认播放速度
    constexpr double SPEED_STEP = 0.1;           // 速度调节步进
    
    // 网络配置常量
    constexpr int DEFAULT_NETWORK_TIMEOUT_SEC = 30;     // 默认网络超时(秒)
    constexpr int MAX_REDIRECTS = 10;                   // 最大重定向次数
    constexpr int DOWNLOAD_BUFFER_SIZE = 8192;          // 下载缓冲区大小
    
    // UI配置常量
    constexpr int MIN_PROGRESS_BAR_WIDTH = 10;          // 最小进度条宽度
    constexpr int MAX_PROGRESS_BAR_WIDTH = 30;          // 最大进度条宽度
    constexpr int PROGRESS_BAR_RESERVED_SPACE = 22;     // 进度条预留空间(速率+ETA)
    constexpr int DEFAULT_LAYOUT_RATIO_NUMERATOR = 4;   // 默认布局比例分子
    constexpr int DEFAULT_LAYOUT_RATIO_DENOMINATOR = 10;// 默认布局比例分母
    constexpr int PAGE_SCROLL_LINES = 10;               // 翻页滚动行数
    constexpr int DOWNLOAD_COMPLETE_DISPLAY_SEC = 5;    // 下载完成后显示时间(秒)
    constexpr int MAX_LOG_ENTRIES = 1024;              // 最大日志条数 (V0.05B9n3e5g2RF8A: 从500改为1024)
    constexpr int SEARCH_CACHE_MAX = 100;               // 搜索缓存最大条数
    constexpr int PODCAST_CACHE_DAYS = 7;               // 播客缓存过期天数
    constexpr int CACHE_EXPIRE_HOURS = 24576;           // 缓存过期时间(小时) (V0.05B9n3e5g2RF8A: 从24改为24576)
    
    //历史记录配置移至INI文件
    // constexpr int HISTORY_MAX_DAYS = 30;      // 已移至INI配置
    // constexpr int HISTORY_MAX_RECORDS = 360;  // 已移至INI配置

    // =========================================================
    //状态栏艺术效果模块化配置
    // 用户可自定义艺术字符，方便个性化定制
    // =========================================================
    // 左侧艺术效果结构: ART_OUTER_LEFT + 版本号 + ART_OUTER_RIGHT
    constexpr const char* ART_OUTER_LEFT   = "🎵 ♫ ";       // 左侧左部艺术字符
    constexpr const char* ART_OUTER_RIGHT  = " ♫ 🎵";       // 左侧右部艺术字符
    constexpr const char* ART_AUTHOR_LABEL = "";            // 右侧作者标签(空=不显示)
    
    // 中间[]样式
    constexpr const char* ART_BRACKET_LEFT  = "[ ";         // 左方括号
    constexpr const char* ART_BRACKET_RIGHT = " ]";         // 右方括号
    
    // 右侧艺术效果结构: ART_RIGHT_PREFIX + 作者时间 + ART_RIGHT_SUFFIX
    constexpr const char* ART_RIGHT_PREFIX = "🎵 ♫ ";       // 右侧左部艺术字符
    constexpr const char* ART_RIGHT_SUFFIX = " ♫ 🎵";       // 右侧右部艺术字符

    // =========================================================
    //XML错误处理 - 前向声明（函数实现在EVENT_LOG宏定义之后）
    // =========================================================
    extern std::string g_last_xml_error;
    extern int g_xml_error_count;
    void xml_error_handler(void* ctx, const char* msg, ...);
    void xml_structured_error_handler(void* ctx, const xmlError* error);
    void reset_xml_error_state();
    std::string get_last_xml_error();

    // =========================================================
    //UI图标风格系统 - 双模式支持（ASCII/Emoji）
    // =========================================================
    // 问题背景：
    //   Emoji在不同终端中显示宽度不一致（1列或2列），导致边框被破坏
    //   wcwidth()返回值 ≠ 实际显示宽度（在Emoji上经常不匹配）
    //
    // V0.05B9n3e5g3h: 恢复ASCII/Emoji切换功能
    //   按U键切换图标风格
    // =========================================================
    
    //安全布局常量
    constexpr int SAFE_BORDER_MARGIN = 1;     // 右边框安全缓冲列
    constexpr int ICON_FIELD_WIDTH = 3;       // 图标区域固定宽度（图标+空格，emoji宽2+空格1=3）
    constexpr int EMOJI_LOGICAL_WIDTH = 2;    // Emoji逻辑宽度
    constexpr int ASCII_LOGICAL_WIDTH = 1;    // ASCII逻辑宽度
    
    // V0.05B9n3e5g3h: 图标风格枚举
    enum class IconStyle { ASCII, EMOJI };
    
    // V0.05B9n3e5g3h: 全局图标风格（默认Emoji）
    static IconStyle g_icon_style = IconStyle::EMOJI;
    
    //图标管理器 - 支持ASCII/Emoji双模式
    // 图标格式：<图标><空格>，与connector之间保持1空格分隔
    class IconManager {
    public:
        // V0.05B9n3e5g3h: 设置图标风格
        static void set_style(IconStyle style) { g_icon_style = style; }
        static IconStyle get_style() { return g_icon_style; }
        static void toggle_style() {
            g_icon_style = (g_icon_style == IconStyle::EMOJI) ? IconStyle::ASCII : IconStyle::EMOJI;
        }
        static std::string get_style_name() {
            return (g_icon_style == IconStyle::EMOJI) ? "Emoji" : "ASCII";
        }
        
        // 节点状态图标
        static std::string get_folder_expanded() {
            return (g_icon_style == IconStyle::EMOJI) ? "▼ " : "v ";
        }
        static std::string get_folder_collapsed() {
            return (g_icon_style == IconStyle::EMOJI) ? "▶ " : "> ";
        }
        static std::string get_marked() {
            return (g_icon_style == IconStyle::EMOJI) ? "🔖 " : "* ";
        }
        static std::string get_failed() {
            return (g_icon_style == IconStyle::EMOJI) ? "✗ " : "x ";
        }
        static std::string get_loading() {
            return (g_icon_style == IconStyle::EMOJI) ? "⏳ " : "~ ";
        }
        
        // 节点类型图标
        static std::string get_radio() {
            return (g_icon_style == IconStyle::EMOJI) ? "🎵 " : "[R]";
        }
        static std::string get_podcast() {
            return (g_icon_style == IconStyle::EMOJI) ? "🎙 " : "[P]";
        }
        static std::string get_video() {
            return (g_icon_style == IconStyle::EMOJI) ? "📺 " : "[V]";
        }
        static std::string get_music() {
            return (g_icon_style == IconStyle::EMOJI) ? "♪ " : "[M]";
        }
        
        // 播放状态图标
        static std::string get_playing() {
            return (g_icon_style == IconStyle::EMOJI) ? "▶" : ">";
        }
        static std::string get_paused() {
            return (g_icon_style == IconStyle::EMOJI) ? "⏸" : "||";
        }
        static std::string get_stopped() {
            return (g_icon_style == IconStyle::EMOJI) ? "⏹" : "[]";
        }
        static std::string get_buffering() {
            return (g_icon_style == IconStyle::EMOJI) ? "⏳" : "~";
        }
        
        // 缓存状态图标
        static std::string get_cached() {
            return (g_icon_style == IconStyle::EMOJI) ? "✓" : "+";
        }
        static std::string get_downloading() {
            return (g_icon_style == IconStyle::EMOJI) ? "↓" : "d";
        }
        
        // 图标显示宽度
        static int get_icon_display_width() {
            return (g_icon_style == IconStyle::EMOJI) ? EMOJI_LOGICAL_WIDTH : ASCII_LOGICAL_WIDTH;
        }
        
        // 图标区域总宽度（包含空格）
        static int get_icon_field_width() {
            return (g_icon_style == IconStyle::EMOJI) ? ICON_FIELD_WIDTH : 2;  // ASCII: 1字符+空格=2
        }
    };

    // =========================================================
    //终端状态保存与恢复
    // =========================================================
    static struct termios g_original_termios;
    static bool g_termios_saved = false;
    static bool g_ncurses_initialized = false;
    static int g_original_lines = 0;
    static int g_original_cols = 0;

    //保存原始终端属性
    void save_terminal_state() {
        if (tcgetattr(STDIN_FILENO, &g_original_termios) == 0) {
            g_termios_saved = true;
        }
        //保存原始终端大小
        struct winsize ws;
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
            g_original_lines = ws.ws_row;
            g_original_cols = ws.ws_col;
        }
    }

    //恢复原始终端属性
    void restore_terminal_state() {
        if (g_termios_saved) {
            tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
            g_termios_saved = false;
        }
    }

    //终端清理函数（供atexit和信号处理调用）
    void tui_cleanup() {
        if (g_ncurses_initialized) {
            endwin();
            g_ncurses_initialized = false;
        }
        // 恢复光标
        printf("\033[?25h");
        // 重置终端属性
        restore_terminal_state();
        fflush(stdout);
        fflush(stderr);
    }

    //全局退出请求标志（用于SIGINT）
    static std::atomic<bool> g_exit_requested{false};

    //信号处理函数
    void signal_handler(int sig) {
        if (sig == SIGINT) {
            //CTRL+C不直接退出，设置标志让主循环处理
            g_exit_requested = true;
            return;
        }
        // 其他信号正常清理退出
        tui_cleanup();
        signal(sig, SIG_DFL);
        raise(sig);
    }

    //注册信号处理
    void setup_signal_handlers() {
        int sigs[] = {SIGINT, SIGTERM, SIGQUIT, SIGSEGV, SIGABRT};
        for (size_t i = 0; i < sizeof(sigs)/sizeof(int); i++) {
            signal(sigs[i], signal_handler);
        }
    }

    // =========================================================
    // 核心类型定义
    // =========================================================
    enum class NodeType { FOLDER, RADIO_STREAM, PODCAST_FEED, PODCAST_EPISODE };
    enum class AppMode { RADIO, PODCAST, FAVOURITE, HISTORY, ONLINE };  //添加ONLINE模式
    enum class AppState { BROWSING, LOADING, BUFFERING, PLAYING, PAUSED, LIST_MODE };  //添加LIST_MODE
    
    //播放模式定义
    enum class PlayMode { 
        SINGLE,   // 单次播放：播放完当前项后停止（默认）
        CYCLE,    // 列表循环：列表播完后从头循环
        RANDOM    // 随机播放：随机选择下一项播放
    };

    struct TreeNode;
    using TreeNodePtr = std::shared_ptr<TreeNode>;
    using TreeNodeWeakPtr = std::weak_ptr<TreeNode>;

    struct TreeNode
    {
        std::string title;
        std::string url;
        NodeType type;
        bool expanded = false;
        bool children_loaded = false;
        bool loading = false;
        bool marked = false;
        bool parse_failed = false;
        std::string error_msg;
        bool is_youtube = false;
        std::string channel_name;
        std::string subtext;
        int duration = 0;
        bool is_cached = false;
        bool is_downloaded = false;
        bool is_db_cached = false;  //数据库缓存标记（来自podcast_cache/episode_cache）
        std::string local_file;
        // V0.02: 播放进度
        double play_position = 0.0;  // 上次播放位置（秒）
        bool play_completed = false; // 是否已播完
        std::vector<std::shared_ptr<TreeNode>> children;

        //LINK支持 - 指向目标节点的引用
        // 当is_link=true时，展开/显示使用linked_node的children
        //统一LINK机制重构
        bool is_link = false;
        TreeNodeWeakPtr linked_node;  // 使用weak_ptr避免循环引用（运行时引用，可能失效）
        std::string link_target_url;  // 链接目标的URL标识（用于持久化和重建）
        std::string source_mode;      // 来源模式: "RADIO", "PODCAST", "ONLINE", "FAVOURITE", "HISTORY"

        //子节点排序顺序
        bool sort_reversed = false;  // true=倒序(新→旧), false=正序(旧→新)

        //父节点指针（用于自动播放下一曲）
        TreeNodeWeakPtr parent;
    };

    //播放列表项结构体
    //扩展添加node_path用于SoftLink
    struct PlaylistItem {
        std::string title;
        std::string url;
        int duration = 0;
        bool is_video = false;
        std::string node_path;  //节点路径（SoftLink引用）
    };

    //持久化播放列表项（用于数据库存储）
    struct SavedPlaylistItem : public PlaylistItem {
        int id = 0;           // 数据库ID
        int sort_order = 0;   // 排序顺序
    };

    // =========================================================
    // URL类型枚举
    // =========================================================
    enum class URLType {
        UNKNOWN, OPML, RSS_PODCAST, YOUTUBE_RSS, YOUTUBE_CHANNEL,
        YOUTUBE_VIDEO, YOUTUBE_PLAYLIST, APPLE_PODCAST, RADIO_STREAM, VIDEO_FILE
    };

    // =========================================================
    // URL分类器
    // =========================================================
    class URLClassifier {
    public:
        static URLType classify(const std::string& url) {
            if (url.empty()) return URLType::UNKNOWN;
            
            // 本地文件
            if (url.find("file://") == 0 || url[0] == '/') return URLType::RADIO_STREAM;
            
            // V2.39: 检测视频文件URL
            if (url.find(".mp4") != std::string::npos || 
                url.find(".webm") != std::string::npos ||
                url.find(".mkv") != std::string::npos ||
                url.find(".avi") != std::string::npos ||
                url.find(".mov") != std::string::npos) {
                return URLType::VIDEO_FILE;
            }
            
            if (url.find("youtube.com/@") != std::string::npos ||
                url.find("youtube.com/channel/") != std::string::npos ||
                url.find("youtube.com/c/") != std::string::npos) return URLType::YOUTUBE_CHANNEL;
            if (url.find("youtube.com/feeds/videos.xml") != std::string::npos) return URLType::YOUTUBE_RSS;
            if (url.find("youtube.com/watch") != std::string::npos ||
                url.find("youtu.be/") != std::string::npos) return URLType::YOUTUBE_VIDEO;
            if (url.find("youtube.com/playlist") != std::string::npos) return URLType::YOUTUBE_PLAYLIST;
            if (url.find("podcasts.apple.com") != std::string::npos) return URLType::APPLE_PODCAST;
            //Tune.ashx 需要区分目录和流
            // c=pbrowse 表示浏览目录，应该是 OPML 类型
            if (url.find("Tune.ashx") != std::string::npos) {
                if (url.find("c=pbrowse") != std::string::npos || 
                    url.find("c=sbrowse") != std::string::npos) {
                    return URLType::OPML;  // 目录浏览
                }
                return URLType::RADIO_STREAM;  // 音频流
            }
            if (url.find(".m3u8") != std::string::npos || url.find(".mp3") != std::string::npos ||
                url.find(".aac") != std::string::npos) return URLType::RADIO_STREAM;
            if (url.find("Browse.ashx") != std::string::npos || url.find(".opml") != std::string::npos) return URLType::OPML;
            if (url.find(".xml") != std::string::npos || url.find("/feed") != std::string::npos ||
                url.find("/rss") != std::string::npos) return URLType::RSS_PODCAST;
            return URLType::UNKNOWN;
        }
        
        static std::string type_name(URLType type) {
            switch (type) {
                case URLType::OPML: return "OPML";
                case URLType::RSS_PODCAST: return "RSS";
                case URLType::YOUTUBE_RSS: return "YouTube RSS";
                case URLType::YOUTUBE_CHANNEL: return "YouTube Channel";
                case URLType::YOUTUBE_VIDEO: return "YouTube Video";
                case URLType::APPLE_PODCAST: return "Apple Podcast";
                case URLType::RADIO_STREAM: return "Radio Stream";
                case URLType::VIDEO_FILE: return "Video File";
                default: return "Unknown";
            }
        }
        
        static bool is_youtube(URLType type) {
            return type == URLType::YOUTUBE_RSS || type == URLType::YOUTUBE_CHANNEL ||
                   type == URLType::YOUTUBE_VIDEO || type == URLType::YOUTUBE_PLAYLIST;
        }
        
        static bool is_video(URLType type) {
            return is_youtube(type) || type == URLType::VIDEO_FILE;
        }
        
        static std::string extract_channel_name(const std::string& url) {
            size_t pos;
            if ((pos = url.find("/@")) != std::string::npos) {
                std::string name = url.substr(pos + 2);
                size_t end = name.find_first_of("/?#");
                if (end != std::string::npos) name = name.substr(0, end);
                return name.empty() ? "YouTube Channel" : name;
            }
            if ((pos = url.find("/channel/")) != std::string::npos) {
                std::string name = url.substr(pos + 9);
                size_t end = name.find_first_of("/?#");
                if (end != std::string::npos) name = name.substr(0, end);
                return name.empty() ? "YouTube Channel" : "ch:" + name.substr(0, std::min((size_t)8, name.length()));
            }
            if ((pos = url.find("/c/")) != std::string::npos) {
                std::string name = url.substr(pos + 3);
                size_t end = name.find_first_of("/?#");
                if (end != std::string::npos) name = name.substr(0, end);
                return name.empty() ? "YouTube Channel" : name;
            }
            return "YouTube Channel";
        }
    };

    // =========================================================
    // 状态栏颜色配置
    // =========================================================
    enum class StatusBarColorMode {
        RAINBOW,
        RANDOM,
        TIME_BRIGHTNESS,
        FIXED,
        CUSTOM
    };

    struct StatusBarColorConfig {
        StatusBarColorMode mode = StatusBarColorMode::RAINBOW;
        int update_interval_ms = 100;
        float brightness_min = 0.5f;
        float brightness_max = 1.0f;
        bool time_adjust = true;
        std::string fixed_color = "cyan";
        int rainbow_speed = 1;
    };

    // =========================================================
    // INI配置管理
    // =========================================================
    class IniConfig {
    public:
        static IniConfig& instance() { static IniConfig ic; return ic; }
        
        void load() {
            std::string path = get_config_file();
            if (!fs::exists(path)) {
                create_default(path);
            }
            
            std::ifstream f(path);
            std::string line;
            std::string current_section;
            
            while (std::getline(f, line)) {
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                
                if (line.empty() || line[0] == '#' || line[0] == ';') continue;
                
                if (line[0] == '[' && line.back() == ']') {
                    current_section = line.substr(1, line.length() - 2);
                    continue;
                }
                
                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    
                    if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                        value = value.substr(1, value.length() - 2);
                    }
                    
                    data_[current_section][key] = value;
                }
            }
        }
        
        std::string get(const std::string& section, const std::string& key, const std::string& default_val = "") const {
            if (data_.count(section) && data_.at(section).count(key)) {
                return data_.at(section).at(key);
            }
            return default_val;
        }
        
        int get_int(const std::string& section, const std::string& key, int default_val = 0) const {
            std::string val = get(section, key);
            if (!val.empty()) {
                try { return std::stoi(val); } catch (...) {}
            }
            return default_val;
        }
        
        float get_float(const std::string& section, const std::string& key, float default_val = 0.0f) const {
            std::string val = get(section, key);
            if (!val.empty()) {
                try { return std::stof(val); } catch (...) {}
            }
            return default_val;
        }
        
        bool get_bool(const std::string& section, const std::string& key, bool default_val = false) const {
            std::string val = get(section, key);
            if (val == "true" || val == "yes" || val == "1") return true;
            if (val == "false" || val == "no" || val == "0") return false;
            return default_val;
        }
        
        StatusBarColorConfig get_statusbar_color_config() {
            StatusBarColorConfig cfg;
            
            std::string mode_str = get("statusbar_color", "mode", "rainbow");
            if (mode_str == "random") cfg.mode = StatusBarColorMode::RANDOM;
            else if (mode_str == "time_brightness") cfg.mode = StatusBarColorMode::TIME_BRIGHTNESS;
            else if (mode_str == "fixed") cfg.mode = StatusBarColorMode::FIXED;
            else if (mode_str == "custom") cfg.mode = StatusBarColorMode::CUSTOM;
            else cfg.mode = StatusBarColorMode::RAINBOW;
            
            cfg.update_interval_ms = get_int("statusbar_color", "update_interval_ms", 100);
            cfg.brightness_min = get_float("statusbar_color", "brightness_min", 0.5f);
            cfg.brightness_max = get_float("statusbar_color", "brightness_max", 1.0f);
            cfg.time_adjust = get_bool("statusbar_color", "time_adjust", true);
            cfg.fixed_color = get("statusbar_color", "fixed_color", "cyan");
            cfg.rainbow_speed = get_int("statusbar_color", "rainbow_speed", 1);
            if (cfg.rainbow_speed < 1) cfg.rainbow_speed = 1;
            if (cfg.rainbow_speed > 10) cfg.rainbow_speed = 10;
            
            return cfg;
        }
        
        //获取搜索缓存最大数量（默认1024）
        int get_search_cache_max() {
            return get_int("storage", "search_cache_max", 1024);
        }
        
        //获取播客缓存过期天数
        int get_podcast_cache_days() {
            return get_int("storage", "podcast_cache_days", 1024);
        }
        
        //获取播放历史最大条数（默认2048）
        int get_history_max_records() {
            return get_int("storage", "history_max_records", 2048);
        }
        
        //获取播放历史最大天数（默认1080天≈3年）
        int get_history_max_days() {
            return get_int("storage", "history_max_days", 1080);
        }
        
        //获取默认搜索地区
        std::string get_default_region() {
            return get("search", "default_region", "US");
        }
        
        //新增配置项
        int get_cache_max() const { return get_int("general", "cache_max", 1024); }
        int get_max_log_entries() const { return get_int("general", "max_log_entries", 2048); }
        int get_cache_expire_hours() const { return get_int("advanced", "cache_expire_hours", 24576); }
        bool get_debug() const { return get_bool("advanced", "debug", false); }
        
        static std::string get_config_file() {
            const char* home = std::getenv("HOME");
            return home ? std::string(home) + CONFIG_DIR + "/config.ini" : "";
        }
        
    private:
        IniConfig() {}
        std::map<std::string, std::map<std::string, std::string>> data_;
        
        void create_default(const std::string& path) {
            fs::create_directories(fs::path(path).parent_path());
            std::ofstream f(path);
            if (f.is_open()) {
                f << R"(# ============================================================
# PODRADIO Configuration File
# Version: )" << VERSION << R"(
# Author: Panic
# ============================================================
#
# 本配置文件由程序自动生成，包含所有可用选项
# 修改后重启程序生效
#
# ============================================================
# 【界面显示配置】
# ============================================================
[display]
# 左右面板比例 (0.2-0.8)
layout_ratio = 0.4
# 是否显示树形连接线
show_tree_lines = true
# 是否启用标题滚动显示
scroll_mode = false

# ============================================================
# 【快捷键绑定】
# ============================================================
[keybindings]
# ===== 导航热键 =====
up = k              # 上移光标
down = j            # 下移光标
quit = q            # 退出程序
help = ?            # 显示帮助
page_up = PageUp    # 向上翻页
page_down = PageDown # 向下翻页
go_top = g          # 跳到顶部
go_bottom = G       # 跳到底部
back = h            # 返回上级/折叠
expand = l          # 展开/播放（同Enter）

# ===== 播放控制 =====
pause = space       # 暂停/继续
vol_up = +          # 音量增加
vol_down = -        # 音量减少
speed_up = ]        # 播放速度加快
speed_down = [      # 播放速度减慢
speed_reset = \     # 重置速度到1.0x

# ===== 订阅管理 =====
add_feed = a        # 添加订阅源
delete = d          # 删除节点
edit = e            # 编辑节点标题/URL
favourite = f       # 收藏/取消收藏
refresh = r         # 重新加载当前节点
download = D        # 下载节目
batch_download = B  # 批量下载已标记项目

# ===== 标记功能 =====
mark = m            # 标记/取消标记
visual = v          # 进入Visual模式
clear_marks = V     # 清除所有标记

# ===== 搜索功能 =====
search = /          # 开始搜索(ESC取消)
next_match = J      # 下一个匹配项
prev_match = K      # 上一个匹配项

# ===== 界面控制 =====
toggle_theme = Ctrl+L  # 切换深色/浅色主题（Ctrl+L，ASCII码12）
toggle_tree = T        # 切换树线显示
toggle_scroll = S      # 切换滚动模式

# ===== 模式切换 =====
mode_radio = R      # 切换RADIO模式
mode_podcast = P    # 切换PODCAST模式
mode_favourite = F  # 切换FAVOURITE模式
mode_history = H    # 切换HISTORY模式
mode_online = O     # 切换ONLINE模式
mode_cycle = M      # 循环切换所有模式

# ===== Online模式专属 =====
online_search = /   # 输入搜索关键词
switch_region = B   # 切换搜索地区

# ============================================================
# 【网络配置】
# ============================================================
[network]
# 网络请求超时时间(秒)
timeout = 30

# ============================================================
# 【存储配置】
# ============================================================
[storage]
# 下载文件保存目录
download_dir = ~/Downloads/PodRadio
# EVENT LOG最大显示条数
max_log_entries = 500
# 日志文件保留最大天数 (默认1024天≈3年)
log_max_days = 1024
# 搜索缓存最大条数（不按时间清理，只按数量保留最新N条）
search_cache_max = 1024
# 播放历史最大条数
history_max_records = 2048
# 播放历史最大天数（约3年，0表示不限制天数）
history_max_days = 1080
# 播客信息缓存过期天数
podcast_cache_days = 1024

# ============================================================
# 【YouTube/视频下载配置】
# ============================================================
[youtube]
# 视频最大分辨率高度 (360/480/720/1080/1440/2160)
video_max_height = 1080
# 视频最大帧率 (24/30/60)
video_max_fps = 30
# 下载模式:
#   merged  - 合并视频+音频为单个文件(推荐)
#   video   - 仅下载视频流
#   audio   - 仅下载音频流(MP3格式)
download_mode = merged
# 音频下载格式: mp3/m4a/opus/ogg
audio_format = mp3
# 音频下载质量: 0(最佳)-9(最差)，推荐0
audio_quality = 0
# 嵌入缩略图
embed_thumbnail = true
# 嵌入字幕
embed_subtitle = false

# ============================================================
# 【状态栏颜色配置】
# ============================================================
[statusbar_color]
# 颜色模式:
#   rainbow  - 彩虹渐变效果
#   random   - 随机颜色
#   fixed    - 固定颜色(使用fixed_color)
#   custom   - 自定义颜色序列
mode = rainbow
# 更新间隔(毫秒)
update_interval_ms = 100
# 亮度范围 (0.0-1.0)
brightness_min = 0.5
brightness_max = 1.0
# 时间调整效果
time_adjust = true
# 固定颜色模式时使用的颜色:
#   black red green yellow blue magenta cyan white
fixed_color = cyan
# 彩虹模式速度 (1-10)
rainbow_speed = 1

# ============================================================
# 【高级配置】
# ============================================================
[advanced]
# 缓存过期时间(小时)
cache_expire_hours = 24
# 调试模式
debug = false

# ============================================================
# 【iTunes搜索配置】
# ============================================================
[search]
# 搜索结果缓存数量（保存在SQLite数据库）
cache_max = 100
# 默认搜索地区:
#   US - 美国
#   CN - 中国
#   TW - 台湾
#   JP - 日本
#   UK - 英国
#   DE - 德国
#   FR - 法国
#   KR - 韩国
#   AU - 澳大利亚
default_region = US

# ============================================================
# 【艺术显示颜色代码参考】
# ============================================================
# ncurses标准颜色代码:
#   0 - 默认颜色
#   1 - 黑色 (black)
#   2 - 红色 (red)
#   3 - 绿色 (green)
#   4 - 黄色 (yellow)
#   5 - 蓝色 (blue)
#   6 - 洋红 (magenta)
#   7 - 青色 (cyan)
#   8 - 白色 (white)
#
# 状态栏颜色模式说明:
#   rainbow  - 自动循环显示所有颜色，形成彩虹效果
#   random   - 每次更新随机选择颜色
#   fixed    - 使用fixed_color指定的单一颜色
#   custom   - 使用自定义颜色序列(需修改源码)
# ============================================================
)";
            }
        }
    };

    // =========================================================
    //统一布局管理器 - 窗口尺寸变化与滚动显示联动
    // =========================================================
    // 设计目标：
    //   1. 统一管理所有UI区域的尺寸计算
    //   2. 检测窗口尺寸变化，自动重置滚动偏移
    //   3. 提供标准的可用净宽度计算接口
    //   4. 确保各模式（RADIO/PODCAST/FAVOURITE/HISTORY）显示一致
    //   5. V0.05B6新增：安全边界机制，防止Emoji宽度不一致导致边框被破坏

    // 使用流程：
    //   1. 每帧开始时调用 check_resize() 检测尺寸变化
    //   2. 如果尺寸变化，自动重置所有滚动偏移
    //   3. 调用各 get_*_width() 接口获取可用净宽度
    //   4. 滚动显示使用统一的偏移管理

    // 安全边界设计原则：
    //   - 实际可绘制区域 = 窗口宽度 - 安全缓冲列
    //   - 图标区域固定宽度 = 3列（空格+图标+空格）
    //   - 所有输出前执行越界裁剪
    // =========================================================
    class LayoutMetrics {
    public:
        // 布局参数
        static constexpr float DEFAULT_LAYOUT_RATIO = 0.4f;
        static constexpr int STATUS_BAR_HEIGHT = 3;
        static constexpr int MIN_CONTENT_WIDTH = 10;
        //安全缓冲常量
        static constexpr int SAFE_MARGIN = SAFE_BORDER_MARGIN;  // 右边框安全缓冲列
        
        // 尺寸结构体
        struct WindowSize {
            int lines = 0;
            int cols = 0;
            bool changed = false;  // 本次检测是否发生变化
        };
        
        struct PanelMetrics {
            int left_w = 0;       // 左面板宽度
            int right_w = 0;      // 右面板宽度
            int top_h = 0;        // 顶部高度（不含状态栏）
            int content_w = 0;    // 左面板内容区宽度（减去边框）
            int safe_content_w = 0; //安全内容区宽度（减去边框和安全缓冲）
            int right_inner_w = 0;// 右面板内部宽度
            int safe_right_w = 0; //安全右面板宽度
            int status_w = 0;     // 状态栏宽度
            int safe_status_w = 0;//安全状态栏宽度
        };
        
    private:
        WindowSize last_size_;
        PanelMetrics metrics_;
        float layout_ratio_ = DEFAULT_LAYOUT_RATIO;
        
        //滚动偏移管理 - 统一管理所有滚动状态
        std::map<int, int> line_scroll_offsets_;  // 左面板每行滚动偏移
        int log_scroll_offset_ = 0;                // 日志滚动偏移
        int status_scroll_offset_ = 0;             // 状态栏URL滚动偏移
        
    public:
        static LayoutMetrics& instance() { static LayoutMetrics lm; return lm; }
        
        //检测窗口尺寸变化 - 核心联动入口
        // 返回值：true表示尺寸发生变化，需要完全重绘
        bool check_resize() {
            int current_lines = LINES;
            int current_cols = COLS;
            
            last_size_.changed = (current_lines != last_size_.lines || 
                                  current_cols != last_size_.cols);
            
            if (last_size_.changed) {
                last_size_.lines = current_lines;
                last_size_.cols = current_cols;
                
                //关键 - 尺寸变化时重置所有滚动偏移
                // 因为可用净宽度变了，旧的滚动位置可能无效
                reset_all_scroll_offsets();
                
                // 重新计算布局
                recalculate_metrics();
            }
            
            return last_size_.changed;
        }
        
        //重新计算布局尺寸（含安全宽度）
        void recalculate_metrics() {
            metrics_.status_w = last_size_.cols;
            metrics_.top_h = last_size_.lines - STATUS_BAR_HEIGHT;
            //使用整数运算避免浮点精度问题
            metrics_.left_w = last_size_.cols * 40 / 100;
            metrics_.right_w = last_size_.cols - metrics_.left_w;
            
            // 内容区宽度 = 窗口宽度 - 左右边框(2)
            metrics_.content_w = metrics_.left_w - 2;
            if (metrics_.content_w < MIN_CONTENT_WIDTH) {
                metrics_.content_w = MIN_CONTENT_WIDTH;
            }
            
            //安全内容区宽度 = 内容区宽度 - 安全缓冲(1)
            // 用于防止Emoji宽度不一致导致边框被破坏
            metrics_.safe_content_w = metrics_.content_w - SAFE_MARGIN;
            if (metrics_.safe_content_w < MIN_CONTENT_WIDTH) {
                metrics_.safe_content_w = MIN_CONTENT_WIDTH;
            }
            
            //修复右面板内部宽度计算
            // 内容区宽度 = right_w - 2（左右边框各1列）
            // 从列2开始打印，可用宽度 = right_w - 3
            metrics_.right_inner_w = metrics_.right_w - 3;
            if (metrics_.right_inner_w < MIN_CONTENT_WIDTH) {
                metrics_.right_inner_w = MIN_CONTENT_WIDTH;
            }
            
            //安全右面板宽度
            metrics_.safe_right_w = metrics_.right_inner_w - SAFE_MARGIN;
            if (metrics_.safe_right_w < MIN_CONTENT_WIDTH) {
                metrics_.safe_right_w = MIN_CONTENT_WIDTH;
            }
            
            //安全状态栏宽度
            metrics_.safe_status_w = metrics_.status_w - SAFE_MARGIN;
        }
        
        //重置所有滚动偏移 - 尺寸变化时调用
        void reset_all_scroll_offsets() {
            line_scroll_offsets_.clear();
            log_scroll_offset_ = 0;
            status_scroll_offset_ = 0;
        }
        
        //获取布局尺寸
        const PanelMetrics& get_metrics() const { return metrics_; }
        const WindowSize& get_window_size() const { return last_size_; }
        
        //设置布局比例
        void set_layout_ratio(float ratio) {
            if (ratio > 0.2f && ratio < 0.8f) {
                layout_ratio_ = ratio;
                recalculate_metrics();
            }
        }
        
        //计算节点行的可用净宽度（使用安全宽度）
        // 参数：depth - 节点深度(保留用于未来扩展), fixed_width - 固定部分宽度（缩进+连接线+图标）
        // 返回：标题可用的显示宽度（已预留安全缓冲）
        int get_title_available_width(int depth, int fixed_width) const {
            (void)depth; // 保留参数供未来使用
            //使用安全内容区宽度，防止Emoji宽度溢出
            int available = metrics_.safe_content_w - fixed_width;
            return (available > 0) ? available : 1;
        }
        
        //计算右面板日志区域的可用净宽度（使用安全宽度）
        // 参数：timestamp_width - 时间戳占用宽度
        int get_log_available_width(int timestamp_width = 14) const {
            //使用安全右面板宽度
            int available = metrics_.safe_right_w - timestamp_width - 1;
            return (available > 0) ? available : 1;
        }
        
        //计算状态栏中间区域的可用净宽度（使用安全宽度）
        // 参数：left_art_w - 左侧艺术字宽度, right_art_w - 右侧艺术字宽度
        int get_status_available_width(int left_art_w, int right_art_w) const {
            //使用安全状态栏宽度
            int available = metrics_.safe_status_w - left_art_w - right_art_w - 4;
            return (available > 10) ? available : 0;
        }
        
        //滚动偏移管理接口
        int get_line_scroll_offset(int line_y) const {
            auto it = line_scroll_offsets_.find(line_y);
            return (it != line_scroll_offsets_.end()) ? it->second : 0;
        }
        
        void increment_line_scroll_offset(int line_y) {
            line_scroll_offsets_[line_y]++;
        }
        
        int get_log_scroll_offset() const { return log_scroll_offset_; }
        void increment_log_scroll_offset() { log_scroll_offset_++; }
        
        int get_status_scroll_offset() const { return status_scroll_offset_; }
        void increment_status_scroll_offset() { status_scroll_offset_++; }
        
        //直接访问滚动偏移表（兼容现有代码）
        std::map<int, int>& get_line_scroll_offsets() { return line_scroll_offsets_; }
    };

    // =========================================================
    // 日志系统
    // =========================================================
    class Logger {
    public:
        static Logger& instance() { static Logger l; return l; }
        
        void init() {
            const char* home = std::getenv("HOME");
            if (home) {
                std::string dir = std::string(home) + DATA_DIR;
                fs::create_directories(dir);
                file_.open(dir + "/podradio.log", std::ios::app);
                if (file_.is_open()) {
                    file_ << "========================================\n";
                    file_ << fmt::format("{} {} by {}\n", APP_NAME, VERSION, AUTHOR);
                    file_ << "========================================\n";
                    file_.flush();
                }
            }
        }
        
        void log(const std::string& msg) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (file_.is_open()) {
                //所有日志条目添加时间戳
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                char buf[32];
                std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
                file_ << fmt::format("[{}.{:03d}] {}", buf, ms.count(), msg) << std::endl;
                file_.flush();
            }
        }
        
        ~Logger() { if (file_.is_open()) { file_ << "========================================\n"; file_.close(); } }
        
    private:
        Logger() = default;
        std::ofstream file_;
        std::mutex mtx_;
    };
    #define LOG(msg) Logger::instance().log(msg)

    // =========================================================
    // 工具类 - UTF-8显示宽度计算
    // =========================================================
    class Utils {
    public:
        static std::string expand_path(const std::string& path) {
            if (path.empty() || path[0] != '~') return path;
            const char* home = std::getenv("HOME");
            return home ? std::string(home) + path.substr(1) : path;
        }
        
        //已移除JSON缓存文件，改用数据库存储
        // static std::string get_data_file() { return expand_path("~" + std::string(DATA_DIR) + "/data.json"); }
        // static std::string get_cache_file() { return expand_path("~" + std::string(DATA_DIR) + "/cache.json"); }
        // static std::string get_youtube_cache_file() { return expand_path("~" + std::string(DATA_DIR) + "/youtube_cache.json"); }
        static std::string get_download_dir() { return expand_path("~" + std::string(DOWNLOAD_DIR)); }
        static std::string get_log_file() { return expand_path("~" + std::string(DATA_DIR) + "/podradio.log"); }
        static std::string to_lower(const std::string& s) { std::string r = s; std::transform(r.begin(), r.end(), r.begin(), ::tolower); return r; }
        
        //HTTP转HTTPS - 安全优先，能用https就不用http
        // 将http://开头的URL转换为https://
        static std::string http_to_https(const std::string& url) {
            if (url.size() > 7 && url.substr(0, 7) == "http://") {
                return "https://" + url.substr(7);
            }
            return url;
        }
        
        static std::string execute_command(const std::string& cmd) {
            std::array<char, 4096> buffer;
            std::string result;
            FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe)) result += buffer.data();
                pclose(pipe);
            }
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
            return result;
        }
        
        static std::string sanitize_filename(const std::string& name) {
            std::string result = name;
            std::replace_if(result.begin(), result.end(), [](char c) { 
                return c == '/' || c == '\\' || c == ':' || c == '*' || 
                       c == '?' || c == '"' || c == '<' || c == '>' || c == '|'; 
            }, '_');
            if (result.length() > 200) result = result.substr(0, 200);
            return result;
        }
        
        //URL编码函数
        static std::string url_encode(const std::string& s) {
            std::string result;
            result.reserve(s.size() * 3);
            for (char c : s) {
                if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    result += c;
                } else {
                    char buf[4];
                    snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
                    result += buf;
                }
            }
            return result;
        }
        
        static bool has_gui() { return std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr; }
        
        static int utf8_char_bytes(unsigned char c) {
            if ((c & 0x80) == 0) return 1;
            if ((c & 0xE0) == 0xC0) return 2;
            if ((c & 0xF0) == 0xE0) return 3;
            if ((c & 0xF8) == 0xF0) return 4;
            return 1;
        }
        
        // =========================================================
        //UTF-8字符显示宽度自动检测 - mk_wcwidth算法
        // 基于Unicode 15.1标准，自动识别所有字符宽度
        // 替代硬编码Unicode范围，支持任意新字符
        // =========================================================
        
        // mk_wcwidth: 根据Unicode码点返回显示宽度
        // 返回值: -1(控制字符), 0(不可见/组合字符), 1(半角), 2(全角)
        static int mk_wcwidth(uint32_t ucs) {
            // NULL和控制字符
            if (ucs == 0) return 0;
            if (ucs < 32 || (ucs >= 0x7F && ucs < 0xA0)) return -1;
            
            // ===== 宽度=0 的字符 (Non-spacing, Enclosing, Format) =====
            // Combining Diacritical Marks
            if (ucs >= 0x0300 && ucs <= 0x036F) return 0;
            // Combining Diacritical Marks Extended
            if (ucs >= 0x1AB0 && ucs <= 0x1AFF) return 0;
            // Combining Diacritical Marks Supplement
            if (ucs >= 0x1DC0 && ucs <= 0x1DFF) return 0;
            // Combining Diacritical Marks for Symbols
            if (ucs >= 0x20D0 && ucs <= 0x20FF) return 0;
            // Combining Half Marks
            if (ucs >= 0xFE20 && ucs <= 0xFE2F) return 0;
            
            // Cyrillic combining marks
            if (ucs >= 0x0483 && ucs <= 0x0489) return 0;
            
            // Arabic combining marks
            if (ucs >= 0x064B && ucs <= 0x065F) return 0;
            if (ucs >= 0x0670 && ucs <= 0x0670) return 0;
            if (ucs >= 0x06D6 && ucs <= 0x06DC) return 0;
            if (ucs >= 0x06DF && ucs <= 0x06E4) return 0;
            if (ucs >= 0x06E7 && ucs <= 0x06E8) return 0;
            if (ucs >= 0x06EA && ucs <= 0x06ED) return 0;
            
            // Devanagari combining
            if (ucs >= 0x0900 && ucs <= 0x0903) return 0;
            if (ucs >= 0x093A && ucs <= 0x093B) return 0;
            if (ucs >= 0x093C && ucs <= 0x093C) return 0;
            if (ucs >= 0x093E && ucs <= 0x094F) return 0;
            if (ucs >= 0x0951 && ucs <= 0x0957) return 0;
            if (ucs >= 0x0962 && ucs <= 0x0963) return 0;
            
            // Hebrew combining
            if (ucs >= 0x0591 && ucs <= 0x05BD) return 0;
            if (ucs >= 0x05BF && ucs <= 0x05BF) return 0;
            if (ucs >= 0x05C1 && ucs <= 0x05C2) return 0;
            if (ucs >= 0x05C4 && ucs <= 0x05C5) return 0;
            if (ucs >= 0x05C7 && ucs <= 0x05C7) return 0;
            
            // Thai combining
            if (ucs >= 0x0E31 && ucs <= 0x0E31) return 0;
            if (ucs >= 0x0E34 && ucs <= 0x0E3A) return 0;
            if (ucs >= 0x0E47 && ucs <= 0x0E4E) return 0;
            
            // Zero-width characters
            if (ucs == 0x200B) return 0;  // Zero Width Space
            if (ucs == 0x200C) return 0;  // Zero Width Non-Joiner
            if (ucs == 0x200D) return 0;  // Zero Width Joiner
            if (ucs == 0x200E) return 0;  // Left-to-Right Mark
            if (ucs == 0x200F) return 0;  // Right-to-Left Mark
            
            // Bidirectional formatting
            if (ucs >= 0x202A && ucs <= 0x202E) return 0;
            if (ucs >= 0x2060 && ucs <= 0x2063) return 0;
            if (ucs >= 0x2066 && ucs <= 0x2069) return 0;
            if (ucs >= 0x206A && ucs <= 0x206F) return 0;
            
            // Variation Selectors
            if (ucs >= 0xFE00 && ucs <= 0xFE0F) return 0;
            if (ucs >= 0xE0100 && ucs <= 0xE01EF) return 0;
            
            // Other format characters
            if (ucs == 0x061C) return 0;  // Arabic Letter Mark
            if (ucs == 0x034F) return 0;  // Combining Grapheme Joiner
            if (ucs == 0x115F) return 0;  // Hangul Choseong Filler
            if (ucs == 0x1160) return 0;  // Hangul Jungseong Filler
            if (ucs >= 0x17B4 && ucs <= 0x17B5) return 0;  // Khmer vowels
            if (ucs >= 0x180B && ucs <= 0x180D) return 0;  // Mongolian FVS
            if (ucs >= 0x180F && ucs <= 0x180F) return 0;
            if (ucs >= 0xFEFF && ucs <= 0xFEFF) return 0;  // BOM/ZWNBSP
            
            // ===== 宽度=2 的字符 (East Asian Wide/F) =====
            
            // Hangul Jamo
            if (ucs >= 0x1100 && ucs <= 0x115F) return 2;
            if (ucs >= 0x1161 && ucs <= 0x11FF) return 2;
            if (ucs >= 0xA960 && ucs <= 0xA97C) return 2;  // Hangul Jamo Extended-A
            if (ucs >= 0xD7B0 && ucs <= 0xD7C6) return 2;  // Hangul Jamo Extended-B
            if (ucs >= 0xD7CB && ucs <= 0xD7FB) return 2;  // Hangul Jamo Extended-B
            
            // CJK Radicals and Symbols
            if (ucs >= 0x2E80 && ucs <= 0x2EF3) return 2;  // CJK Radicals Supplement
            if (ucs >= 0x2F00 && ucs <= 0x2FD5) return 2;  // Kangxi Radicals
            if (ucs >= 0x2FF0 && ucs <= 0x2FFB) return 2;  // Ideographic Description
            
            // CJK Symbols and Punctuation
            if (ucs >= 0x3000 && ucs <= 0x303E) return 2;
            if (ucs >= 0x3001 && ucs <= 0x3003) return 2;
            if (ucs == 0x303F) return 1;  // Halfwidth space
            if (ucs >= 0x301C && ucs <= 0x301D) return 2;
            
            // Hiragana
            if (ucs >= 0x3040 && ucs <= 0x309F) return 2;
            
            // Katakana
            if (ucs >= 0x30A0 && ucs <= 0x30FF) return 2;
            
            // Bopomofo
            if (ucs >= 0x3100 && ucs <= 0x312F) return 2;
            if (ucs >= 0x31A0 && ucs <= 0x31BF) return 2;  // Bopomofo Extended
            
            // CJK Strokes
            if (ucs >= 0x31C0 && ucs <= 0x31E3) return 2;
            
            // CJK Unified Ideographs
            if (ucs >= 0x3400 && ucs <= 0x4DBF) return 2;  // Extension A
            if (ucs >= 0x4E00 && ucs <= 0x9FFF) return 2;  // Main block
            if (ucs >= 0xA000 && ucs <= 0xA48C) return 2;  // Yi Syllables
            if (ucs >= 0xA490 && ucs <= 0xA4C6) return 2;  // Yi Radicals
            
            // Hangul Syllables
            if (ucs >= 0xAC00 && ucs <= 0xD7A3) return 2;
            
            // CJK Compatibility Ideographs
            if (ucs >= 0xF900 && ucs <= 0xFAFF) return 2;
            
            // Vertical Forms
            if (ucs >= 0xFE10 && ucs <= 0xFE19) return 2;
            if (ucs >= 0xFE30 && ucs <= 0xFE52) return 2;  // CJK Compatibility Forms
            if (ucs >= 0xFE54 && ucs <= 0xFE66) return 2;
            if (ucs >= 0xFE68 && ucs <= 0xFE6B) return 2;
            
            // Fullwidth Forms
            if (ucs >= 0xFF01 && ucs <= 0xFF60) return 2;  // Fullwidth ASCII
            if (ucs >= 0xFFE0 && ucs <= 0xFFE6) return 2;  // Fullwidth Symbols
            
            // Angle brackets (Wide)
            if (ucs == 0x2329 || ucs == 0x232A) return 2;
            if (ucs == 0x3008 || ucs == 0x3009) return 2;  // CJK angle brackets
            
            // Currency symbols that are wide
            if (ucs == 0x00A2) return 2;  // Cent sign
            if (ucs == 0x00A3) return 2;  // Pound sign
            if (ucs == 0x00A5) return 2;  // Yen sign
            if (ucs == 0x00A6) return 1;  // Broken bar (narrow)
            if (ucs == 0x00AC) return 2;  // Not sign (wide in some fonts)
            if (ucs == 0x20A9) return 2;  // Won sign
            
            // Greek letter substitutes (Wide)
            if (ucs >= 0x0391 && ucs <= 0x03A9) return 2;  // Greek Capital
            if (ucs >= 0x03B1 && ucs <= 0x03C9) return 2;  // Greek Small
            if (ucs >= 0x03D0 && ucs <= 0x03F5) return 2;  // Greek Extended
            
            // Cyrillic (usually narrow, but some are wide in East Asian)
            // Keep narrow for Cyrillic
            
            // CJK Extensions B-G (Supplementary Planes)
            if (ucs >= 0x20000 && ucs <= 0x2A6DF) return 2;  // Extension B
            if (ucs >= 0x2A700 && ucs <= 0x2B73F) return 2;  // Extension C
            if (ucs >= 0x2B740 && ucs <= 0x2B81F) return 2;  // Extension D
            if (ucs >= 0x2B820 && ucs <= 0x2CEAF) return 2;  // Extension E
            if (ucs >= 0x2CEB0 && ucs <= 0x2EBEF) return 2;  // Extension F
            if (ucs >= 0x2F800 && ucs <= 0x2FA1F) return 2;  // Compatibility Supplement
            if (ucs >= 0x30000 && ucs <= 0x3134F) return 2;  // Extension G
            if (ucs >= 0x31350 && ucs <= 0x323AF) return 2;  // Extension H
            
            // ===== V0.05B9n3e5g: Emoji宽度=2 =====
            // 根本原因：emoji在终端中实际显示宽度为2列
            // 但mk_wcwidth默认返回1，导致截断计算错误

            // Emoji主要Unicode范围：
            if (ucs >= 0x1F300 && ucs <= 0x1F5FF) return 2;  // Miscellaneous Symbols and Pictographs (🎯在此: U+1F3AF)
            if (ucs >= 0x1F600 && ucs <= 0x1F64F) return 2;  // Emoticons
            if (ucs >= 0x1F680 && ucs <= 0x1F6FF) return 2;  // Transport and Map Symbols
            if (ucs >= 0x1F900 && ucs <= 0x1F9FF) return 2;  // Supplemental Symbols and Pictographs
            if (ucs >= 0x1FA00 && ucs <= 0x1FAFF) return 2;  // Extended Pictographs
            if (ucs >= 0x2600 && ucs <= 0x26FF) return 2;    // Miscellaneous Symbols
            if (ucs >= 0x2700 && ucs <= 0x27BF) return 2;    // Dingbats
            if (ucs >= 0x2300 && ucs <= 0x23FF) return 2;    // Miscellaneous Technical (⏳⏸⏹等)
            if (ucs >= 0x25A0 && ucs <= 0x25FF) return 2;    // Geometric Shapes (▶◀▼▲等)
            if (ucs >= 0x2500 && ucs <= 0x259F) return 1;    // Box Drawing (边框字符，宽度1)
            
            // ===== 宽度=1 的字符 (默认) =====
            // 包括: ASCII, Latin Extended, Cyrillic, 
            // Box Drawing, Block Elements,
            // Mathematical Symbols, Arrows
            
            return 1;
        }
        
        //使用mk_wcwidth的UTF-8字符宽度检测
        // 自动识别所有Unicode字符宽度，无需硬编码范围
        static int utf8_char_display_width(unsigned char first_byte, const unsigned char* next_bytes = nullptr) {
            // ASCII字符：直接返回
            if ((first_byte & 0x80) == 0) {
                return mk_wcwidth(first_byte);
            }
            
            // 解码UTF-8码点
            uint32_t codepoint = 0;
            
            // 2字节UTF-8
            if ((first_byte & 0xE0) == 0xC0) {
                codepoint = (first_byte & 0x1F) << 6;
                if (next_bytes) {
                    codepoint |= (next_bytes[0] & 0x3F);
                }
                return mk_wcwidth(codepoint);
            }
            
            // 3字节UTF-8
            if ((first_byte & 0xF0) == 0xE0) {
                codepoint = (first_byte & 0x0F) << 12;
                if (next_bytes) {
                    codepoint |= (next_bytes[0] & 0x3F) << 6;
                    codepoint |= (next_bytes[1] & 0x3F);
                }
                return mk_wcwidth(codepoint);
            }
            
            // 4字节UTF-8 (emoji, 补充字符等)
            if ((first_byte & 0xF8) == 0xF0) {
                codepoint = (first_byte & 0x07) << 18;
                if (next_bytes) {
                    codepoint |= (next_bytes[0] & 0x3F) << 12;
                    codepoint |= (next_bytes[1] & 0x3F) << 6;
                    codepoint |= (next_bytes[2] & 0x3F);
                }
                return mk_wcwidth(codepoint);
            }
            
            // 无效UTF-8
            return 1;
        }
        
        // V0.02: 改进的显示宽度计算函数
        static int utf8_display_width(const std::string& s) {
            int width = 0;
            for (size_t i = 0; i < s.size(); ) {
                unsigned char c = s[i];
                int char_bytes = utf8_char_bytes(c);

                // 获取后续字节用于完整解码
                const unsigned char* next = (i + 1 < s.size()) ?
                    reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;

                width += utf8_char_display_width(c, next);
                i += char_bytes;
            }
            return width;
        }
        
        // V0.04a: 缩略显示函数，使用"..."作为省略标记
        //修复中文字符宽度计算 - 必须传递next_bytes参数
        static std::string truncate_by_display_width(const std::string& s, int max_display_width) {
            if (max_display_width <= 0) return "";
            
            int current_width = 0;
            size_t i = 0;
            
            while (i < s.size()) {
                unsigned char c = s[i];
                int char_bytes = utf8_char_bytes(c);
                //传递next_bytes以正确计算中文字符宽度（宽度=2）
                const unsigned char* next = (i + 1 < s.size()) ?
                    reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
                int char_width = utf8_char_display_width(c, next);
                
                if (current_width + char_width > max_display_width) break;
                
                current_width += char_width;
                i += char_bytes;
            }
            
            std::string result = s.substr(0, i);
            if (i < s.size()) {
                // V0.04a: 统一使用"..."作为省略标记（三个点，宽度=3）
                if (current_width + 3 <= max_display_width) {
                    result += "...";
                } else if (current_width + 2 <= max_display_width) {
                    result += "..";
                } else if (current_width + 1 <= max_display_width) {
                    result += ".";
                }
            }
            
            return result;
        }
        
        //从右侧截断文本，保留末尾部分
        // 用于状态栏右侧内容的响应式显示
        static std::string truncate_by_display_width_right(const std::string& s, int max_display_width) {
            if (max_display_width <= 0) return "";
            
            int text_width = utf8_display_width(s);
            if (text_width <= max_display_width) return s;
            
            // 需要从前面省略，保留末尾部分
            int skip_width = text_width - max_display_width;
            
            // 找到跳过skip_width后的起始位置
            int current_width = 0;
            size_t start_pos = 0;
            
            for (size_t i = 0; i < s.size(); ) {
                unsigned char c = s[i];
                int char_bytes = utf8_char_bytes(c);
                const unsigned char* next = (i + 1 < s.size()) ?
                    reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
                int char_width = utf8_char_display_width(c, next);
                
                current_width += char_width;
                i += char_bytes;
                
                if (current_width >= skip_width) {
                    start_pos = i;
                    break;
                }
            }
            
            return s.substr(start_pos);
        }
        
        //从中间截断文本，保留首尾部分
        // 用于状态栏中间URL内容的响应式显示
        // 例：truncate_middle("https://example.com/path", 15) -> "https://.../path"
        static std::string truncate_middle(const std::string& s, int max_display_width) {
            if (max_display_width <= 0) return "";
            
            int text_width = utf8_display_width(s);
            if (text_width <= max_display_width) return s;
            
            // 需要从中间省略，保留首尾部分
            // "..." 占3列，剩余空间平分给首尾
            int remaining = max_display_width - 3;  // 减去"..."的宽度
            if (remaining < 2) return "...";  // 极端情况只显示省略号
            
            int head_width = remaining / 2;
            int tail_width = remaining - head_width;
            
            // 提取头部（从头开始）
            std::string head;
            int current_width = 0;
            size_t i = 0;
            while (i < s.size() && current_width < head_width) {
                unsigned char c = s[i];
                int char_bytes = utf8_char_bytes(c);
                const unsigned char* next = (i + 1 < s.size()) ?
                    reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
                int char_width = utf8_char_display_width(c, next);
                
                if (current_width + char_width > head_width) break;
                
                head += s[i];
                current_width += char_width;
                i += char_bytes;
            }
            
            // 提取尾部（从末尾往回）
            std::string tail;
            current_width = 0;
            size_t j = s.size();
            while (j > i && current_width < tail_width) {
                j--;
                // 回退找到UTF-8字符起始字节
                while (j > 0 && (s[j] & 0xC0) == 0x80) j--;
                
                unsigned char c = s[j];
                int char_bytes = utf8_char_bytes(c);
                const unsigned char* next = (j + 1 < s.size()) ?
                    reinterpret_cast<const unsigned char*>(s.c_str() + j + 1) : nullptr;
                int char_width = utf8_char_display_width(c, next);
                
                if (current_width + char_width > tail_width) break;
                
                tail = s.substr(j, char_bytes) + tail;
                current_width += char_width;
            }
            
            return head + "..." + tail;
        }
        
        //文本换行函数
        // 将文本按指定宽度换行，返回多行文本数组
        // max_lines: 最大行数，超过则最后一行用"..."缩略
        static std::vector<std::string> wrap_text(const std::string& s, int max_width, int max_lines = 12) {
            std::vector<std::string> lines;
            if (max_width <= 0 || s.empty()) return lines;
            
            size_t i = 0;
            while (i < s.size() && (int)lines.size() < max_lines) {
                int current_width = 0;
                std::string line;
                
                // 构建一行，不超过max_width
                while (i < s.size()) {
                    unsigned char c = s[i];
                    int char_bytes = utf8_char_bytes(c);
                    const unsigned char* next = (i + 1 < s.size()) ?
                        reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
                    int char_width = utf8_char_display_width(c, next);
                    
                    if (current_width + char_width > max_width) break;
                    
                    line += s.substr(i, char_bytes);
                    current_width += char_width;
                    i += char_bytes;
                }
                
                // 如果是最后一行且还有剩余内容，添加"..."
                if ((int)lines.size() == max_lines - 1 && i < s.size()) {
                    int dots = std::min(3, max_width - current_width);
                    if (dots > 0) {
                        line += std::string(dots, '.');
                    }
                    // 填满最后一行
                    while (current_width + dots < max_width && i < s.size()) {
                        unsigned char c = s[i];
                        int char_bytes = utf8_char_bytes(c);
                        const unsigned char* next = (i + 1 < s.size()) ?
                            reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
                        int char_width = utf8_char_display_width(c, next);
                        if (current_width + dots + char_width > max_width) break;
                        line += s.substr(i, char_bytes);
                        current_width += char_width;
                        i += char_bytes;
                    }
                }
                
                if (!line.empty()) {
                    lines.push_back(line);
                }
            }
            
            return lines;
        }
        
        //工业级滚动文本函数
        // 核心原则：按"终端显示列宽"滚动，而非字符索引
        // 参考：ncurses/vim/tmux的Unicode滚动实现
        static std::string get_scrolling_text(const std::string& text, int max_width, int scroll_offset) {
            if (max_width <= 0) return "";
            
            int text_width = utf8_display_width(text);
            if (text_width <= max_width) return text;
            
            //构建循环滚动的内容
            // 格式：原文本 + "   " + 原文本开头部分（用于无缝循环）
            std::string separator = "   ";
            std::string padded = text + separator;
            int padded_width = utf8_display_width(padded);
            
            //offset是以"终端列"为单位的滚动偏移
            // 使用模运算实现循环滚动
            int offset = scroll_offset % padded_width;
            if (offset < 0) offset = 0;
            
            //Step 1 - 按终端列宽定位起始字节位置
            // 关键：当offset落在宽字符内部时，跳到下一个字符
            int current_width = 0;
            size_t start_pos = 0;
            
            for (size_t i = 0; i < padded.size(); ) {
                unsigned char c = padded[i];
                int char_bytes = utf8_char_bytes(c);
                const unsigned char* next = (i + 1 < padded.size()) ?
                    reinterpret_cast<const unsigned char*>(padded.c_str() + i + 1) : nullptr;
                int char_width = utf8_char_display_width(c, next);
                
                //如果当前字符会让宽度超过offset，则从下一个字符开始
                // 这样可以避免在宽字符中间"切断"
                if (current_width + char_width > offset) {
                    // offset落在此字符内部，跳到此字符之后
                    break;
                }
                
                current_width += char_width;
                i += char_bytes;
                start_pos = i;
            }
            
            //Step 2 - 从start_pos开始，按终端列宽精确截取
            // 确保输出严格不超过max_width列
            std::string remaining = padded.substr(start_pos);
            
            // 如果剩余内容不足max_width，从开头补充（实现无缝循环）
            int remaining_width = utf8_display_width(remaining);
            if (remaining_width < max_width) {
                remaining += text;  // 追加原文本开头
            }
            
            //Step 3 - 严格按max_width截断输出
            return truncate_by_display_width_strict(remaining, max_width);
        }
        
        //严格截断函数 - 输出宽度绝不超过max_width
        static std::string truncate_by_display_width_strict(const std::string& s, int max_display_width) {
            if (max_display_width <= 0) return "";
            
            int current_width = 0;
            size_t i = 0;
            
            while (i < s.size()) {
                unsigned char c = s[i];
                int char_bytes = utf8_char_bytes(c);
                const unsigned char* next = (i + 1 < s.size()) ?
                    reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
                int char_width = utf8_char_display_width(c, next);
                
                //严格检查 - 如果添加此字符会超出，立即停止
                if (current_width + char_width > max_display_width) break;
                
                current_width += char_width;
                i += char_bytes;
            }
            
            return s.substr(0, i);
        }
        
        static std::string format_duration(int seconds) {
            if (seconds <= 0) return "";
            int h = seconds / 3600;
            int m = (seconds % 3600) / 60;
            int s = seconds % 60;
            if (h > 0) return fmt::format("{}h{}m", h, m);
            if (m > 0) return fmt::format("{}m", m);
            return fmt::format("{}s", s);
        }
        
        static std::string format_size(size_t bytes) {
            const char* units[] = {"B", "KB", "MB", "GB"};
            int unit_idx = 0;
            double size = static_cast<double>(bytes);
            while (size >= 1024.0 && unit_idx < 3) { size /= 1024.0; unit_idx++; }
            return fmt::format("{:.1f}{}", size, units[unit_idx]);
        }
        
        static size_t get_file_size(const std::string& path) {
            try {
                if (fs::exists(path)) return fs::file_size(path);
            } catch (...) {}
            return 0;
        }
        
        static bool is_partial_download(const std::string& path) {
            size_t pos = path.find_last_of('.');
            if (pos != std::string::npos) {
                std::string ext = path.substr(pos);
                if (ext == ".part" || ext == ".tmp" || ext == ".partial") return true;
            }
            return fs::exists(path + ".part");
        }
        
        static std::string find_downloaded_file(const std::string& dir, const std::string& base_name) {
            std::vector<std::string> extensions = {".mp4", ".mp3", ".m4a", ".webm", ".mkv", ".opus"};
            for (const auto& ext : extensions) {
                std::string path = dir + "/" + base_name + ext;
                if (fs::exists(path)) return path;
            }
            return "";
        }
    };

    // =========================================================
    //下载进度管理 - 增强版
    //改进已完成下载项的显示和清理
    // =========================================================
    struct DownloadProgress {
        std::string id;
        std::string title;
        std::string url;
        std::string status;
        int percent = 0;
        std::string speed;
        int eta_seconds = 0;          //剩余时间估算（秒）
        int64_t downloaded_bytes = 0; //已下载字节数
        int64_t total_bytes = 0;      //总字节数
        bool active = true;
        bool complete = false;
        bool failed = false;
        bool is_youtube = false;
        std::chrono::steady_clock::time_point complete_time;  //完成时间戳
    };
    
    class ProgressManager {
    public:
        static ProgressManager& instance() { static ProgressManager pm; return pm; }
        
        std::string start_download(const std::string& title, const std::string& url, bool is_youtube = false) {
            std::lock_guard<std::mutex> lock(mtx_);
            std::string id = fmt::format("dl_{}", counter_++);
            downloads_[id] = {id, title, url, "Starting...", 0, "", 0, 0, 0, true, false, false, is_youtube, std::chrono::steady_clock::now()};
            return id;
        }
        
        //增强版update，支持ETA和字节统计
        void update(const std::string& id, int percent, const std::string& status, const std::string& speed = "",
                    int eta_seconds = 0, int64_t downloaded_bytes = 0, int64_t total_bytes = 0) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (downloads_.count(id)) {
                downloads_[id].percent = percent;
                downloads_[id].status = status;
                downloads_[id].speed = speed;
                downloads_[id].eta_seconds = eta_seconds;
                downloads_[id].downloaded_bytes = downloaded_bytes;
                downloads_[id].total_bytes = total_bytes;
            }
        }
        
        //完成时记录时间戳
        void complete(const std::string& id, bool success) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (downloads_.count(id)) {
                downloads_[id].active = false;
                downloads_[id].complete = true;
                downloads_[id].failed = !success;
                downloads_[id].percent = 100;
                downloads_[id].status = success ? "Complete" : "Failed";
                downloads_[id].eta_seconds = 0;
                downloads_[id].complete_time = std::chrono::steady_clock::now();
                downloads_[id].speed = success ? "Done" : "Error";
            }
        }
        
        //改进清理逻辑，已完成的下载保留5秒后再清理
        std::vector<DownloadProgress> get_all() {
            std::lock_guard<std::mutex> lock(mtx_);
            std::vector<DownloadProgress> result;
            auto now = std::chrono::steady_clock::now();
            
            for (auto& [id, p] : downloads_) {
                result.push_back(p);
            }
            
            //清理已完成后超过指定时间的下载项
            for (auto it = downloads_.begin(); it != downloads_.end(); ) {
                if (it->second.complete && !it->second.active) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.complete_time).count();
                    if (elapsed > DOWNLOAD_COMPLETE_DISPLAY_SEC) {
                        it = downloads_.erase(it);
                        continue;
                    }
                }
                ++it;
            }
            return result;
        }
        
        //获取活动下载数量
        int get_active_count() {
            std::lock_guard<std::mutex> lock(mtx_);
            int count = 0;
            for (const auto& [id, p] : downloads_) {
                if (p.active) count++;
            }
            return count;
        }
        
    private:
        ProgressManager() {}
        std::map<std::string, DownloadProgress> downloads_;
        std::mutex mtx_;
        int counter_ = 0;
    };

    // =========================================================
    //下载进度回调辅助
    // =========================================================
    // 进度回调用户数据结构
    struct CurlProgressData {
        std::string dl_id;           // ProgressManager下载ID
        std::string title;           // 下载标题
        int64_t last_bytes = 0;      // 上次已下载字节数
        std::chrono::steady_clock::time_point last_time;  // 上次更新时间
    };

    // 格式化下载速度
    inline std::string format_speed(double bytes_per_sec) {
        if (bytes_per_sec < 0 || std::isnan(bytes_per_sec) || std::isinf(bytes_per_sec)) {
            return "...";
        }
        if (bytes_per_sec < 1024) {
            return fmt::format("{:.0f}B/s", bytes_per_sec);
        } else if (bytes_per_sec < 1024 * 1024) {
            return fmt::format("{:.1f}KB/s", bytes_per_sec / 1024);
        } else if (bytes_per_sec < 1024 * 1024 * 1024) {
            return fmt::format("{:.1f}MB/s", bytes_per_sec / (1024 * 1024));
        } else {
            return fmt::format("{:.1f}GB/s", bytes_per_sec / (1024 * 1024 * 1024));
        }
    }

    // 格式化ETA时间
    inline std::string format_eta(int seconds) {
        if (seconds <= 0 || seconds > 86400) return "--:--";
        int hours = seconds / 3600;
        int mins = (seconds % 3600) / 60;
        int secs = seconds % 60;
        if (hours > 0) {
            return fmt::format("{}:{:02d}:{:02d}", hours, mins, secs);
        } else {
            return fmt::format("{}:{:02d}", mins, secs);
        }
    }

    // libcurl进度回调函数（CURLOPT_XFERINFOFUNCTION）
    static int curl_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                       curl_off_t ultotal, curl_off_t ulnow) {
        (void)ultotal; // libcurl标准参数，暂不使用
        (void)ulnow;   // libcurl标准参数，暂不使用
        CurlProgressData* data = static_cast<CurlProgressData*>(clientp);
        if (!data || data->dl_id.empty()) return 0;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - data->last_time).count();

        // 每200ms更新一次进度，避免频繁更新
        if (elapsed < 200 && dlnow < dltotal) return 0;

        int percent = 0;
        if (dltotal > 0) {
            percent = static_cast<int>((dlnow * 100) / dltotal);
            if (percent > 100) percent = 100;
        }

        // 计算下载速度
        double bytes_per_sec = 0;
        int eta_seconds = 0;
        if (elapsed > 0) {
            int64_t bytes_diff = dlnow - data->last_bytes;
            bytes_per_sec = (bytes_diff * 1000.0) / elapsed;
            
            // 计算ETA
            if (bytes_per_sec > 0 && dltotal > dlnow) {
                int64_t remaining_bytes = dltotal - dlnow;
                eta_seconds = static_cast<int>(remaining_bytes / bytes_per_sec);
            }
        }

        // 更新进度
        std::string speed_str = format_speed(bytes_per_sec);
        ProgressManager::instance().update(
            data->dl_id, percent, 
            fmt::format("{}", format_eta(eta_seconds)),
            speed_str, eta_seconds, 
            dlnow, dltotal
        );

        // 更新上次状态
        data->last_bytes = dlnow;
        data->last_time = now;

        return 0;
    }

    // =========================================================
    // Event Log
    // =========================================================
    struct LogEntry {
        std::string timestamp;
        std::string message;
    };

    class EventLog {
    public:
        static EventLog& instance() { static EventLog el; return el; }
        
        void push(const std::string& msg) {
            std::lock_guard<std::mutex> lock(mtx_);
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
            
            LogEntry entry;
            entry.timestamp = fmt::format("[{}.{:03d}]", buf, ms.count());
            entry.message = msg;
            
            logs_.push_front(entry);
            while (logs_.size() > 1024) logs_.pop_back();  //从500改为1024
            
            LOG(fmt::format("[{}.{:03d}] {}", buf, ms.count(), msg));
        }
        
        std::deque<LogEntry> get() {
            std::lock_guard<std::mutex> lock(mtx_);
            return logs_;
        }
        
        size_t size() {
            std::lock_guard<std::mutex> lock(mtx_);
            return logs_.size();
        }
        
    private:
        EventLog() {}
        std::deque<LogEntry> logs_;
        std::mutex mtx_;
    };
    #define EVENT_LOG(msg) EventLog::instance().push(msg)

    // =========================================================
    //XML错误处理函数实现（必须在LOG/EVENT_LOG宏定义之后）
    // 不再输出到终端，避免花屏
    // =========================================================
    std::string g_last_xml_error;  // 存储最后的XML错误信息
    int g_xml_error_count = 0;     // 错误计数
    
    void xml_error_handler(void* ctx, const char* msg, ...) {
        (void)ctx; // libxml2标准参数，暂不使用
        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);
        
        // 记录到LOG文件
        std::string err_msg = buffer;
        // 移除末尾的换行符
        while (!err_msg.empty() && (err_msg.back() == '\n' || err_msg.back() == '\r')) {
            err_msg.pop_back();
        }
        
        if (!err_msg.empty()) {
            LOG(fmt::format("[XML] {}", err_msg));
            g_last_xml_error = err_msg;
            g_xml_error_count++;
            
            // 只显示第一行错误到EVENT_LOG（避免刷屏）
            if (g_xml_error_count == 1) {
                EVENT_LOG(fmt::format("XML Error: {}", err_msg.substr(0, 60)));
            }
        }
    }
    
    //修正参数类型为const xmlError*（符合libxml2的xmlStructuredErrorFunc定义）
    void xml_structured_error_handler(void* ctx, const xmlError* error) {
        (void)ctx; // libxml2标准参数，暂不使用
        if (error && error->message) {
            std::string msg = error->message;
            // 移除末尾的换行符
            while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) {
                msg.pop_back();
            }
            
            LOG(fmt::format("[XML] Line {}: {}", error->line, msg));
            g_last_xml_error = msg;
            g_xml_error_count++;
            
            if (g_xml_error_count == 1) {
                EVENT_LOG(fmt::format("XML Error (L{}): {}", error->line, msg.substr(0, 50)));
            }
        }
    }
    
    void reset_xml_error_state() {
        g_last_xml_error.clear();
        g_xml_error_count = 0;
    }
    
    std::string get_last_xml_error() {
        return g_last_xml_error;
    }

    // =========================================================
    //SQLite3数据库管理器 - 单例模式，自动清理历史
    // =========================================================
    class DatabaseManager {
    public:
        static DatabaseManager& instance() { static DatabaseManager db; return db; }
        
        bool init() {
            //单例初始化检查，防止多次初始化
            if (initialized_) return true;
            
            const char* home = std::getenv("HOME");
            if (!home) return false;
            
            std::string db_path = std::string(home) + DATA_DIR + "/podradio.db";
            fs::create_directories(fs::path(db_path).parent_path());
            
            if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
                LOG(fmt::format("[DB] Failed to open: {}", sqlite3_errmsg(db_)));
                return false;
            }
            
            // 创建表
            const char* create_tables = R"(
                CREATE TABLE IF NOT EXISTS nodes (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    title TEXT,
                    url TEXT UNIQUE,
                    type INTEGER,
                    parent_id INTEGER,
                    expanded INTEGER DEFAULT 0,
                    data_json TEXT,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE TABLE IF NOT EXISTS progress (
                    url TEXT PRIMARY KEY,
                    position REAL DEFAULT 0,
                    completed INTEGER DEFAULT 0,
                    last_played TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE TABLE IF NOT EXISTS history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    url TEXT,
                    title TEXT,
                    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    duration INTEGER DEFAULT 0
                );
                CREATE TABLE IF NOT EXISTS stats (
                    key TEXT PRIMARY KEY,
                    value TEXT
                );
                CREATE TABLE IF NOT EXISTS cache (
                    url TEXT PRIMARY KEY,
                    data_json TEXT,
                    timestamp INTEGER DEFAULT 0
                );
                CREATE TABLE IF NOT EXISTS search_cache (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    query TEXT NOT NULL,
                    region TEXT DEFAULT 'US',
                    result_json TEXT,
                    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE TABLE IF NOT EXISTS player_state (
                    id INTEGER PRIMARY KEY CHECK (id = 1),
                    volume INTEGER DEFAULT 100,
                    speed REAL DEFAULT 1.0,
                    paused INTEGER DEFAULT 0,
                    current_url TEXT,
                    position REAL DEFAULT 0,
                    scroll_mode INTEGER DEFAULT 0,
                    show_tree_lines INTEGER DEFAULT 1,
                    current_title TEXT,
                    current_mode INTEGER DEFAULT 0,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE TABLE IF NOT EXISTS podcast_cache (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    feed_url TEXT UNIQUE NOT NULL,
                    title TEXT,
                    artist TEXT,
                    genre TEXT,
                    track_count INTEGER DEFAULT 0,
                    artwork_url TEXT,
                    collection_id INTEGER,
                    data_json TEXT,
                    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE TABLE IF NOT EXISTS episode_cache (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    feed_url TEXT NOT NULL,
                    episode_url TEXT,
                    title TEXT,
                    duration INTEGER DEFAULT 0,
                    pub_date TEXT,
                    data_json TEXT,
                    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE TABLE IF NOT EXISTS favourites (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    title TEXT,
                    url TEXT UNIQUE,
                    type INTEGER DEFAULT 0,
                    is_youtube INTEGER DEFAULT 0,
                    channel_name TEXT,
                    source_type TEXT,
                    data_json TEXT,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                -- V0.05B9n3e5g2RF8A: 新增数据库表
                CREATE TABLE IF NOT EXISTS radio_cache (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    parent_id INTEGER DEFAULT 0,
                    title TEXT,
                    url TEXT,
                    type INTEGER DEFAULT 0,
                    expanded INTEGER DEFAULT 0,
                    children_loaded INTEGER DEFAULT 0,
                    is_youtube INTEGER DEFAULT 0,
                    channel_name TEXT,
                    is_cached INTEGER DEFAULT 0,
                    is_downloaded INTEGER DEFAULT 0,
                    local_file TEXT,
                    sort_order INTEGER DEFAULT 0,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE TABLE IF NOT EXISTS youtube_cache (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    channel_url TEXT UNIQUE NOT NULL,
                    channel_name TEXT,
                    videos_json TEXT,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE TABLE IF NOT EXISTS url_cache (
                    url TEXT PRIMARY KEY,
                    is_cached INTEGER DEFAULT 0,
                    is_downloaded INTEGER DEFAULT 0,
                    local_file TEXT,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE INDEX IF NOT EXISTS idx_nodes_url ON nodes(url);
                CREATE INDEX IF NOT EXISTS idx_history_timestamp ON history(timestamp);
                CREATE INDEX IF NOT EXISTS idx_search_query ON search_cache(query, region);
                CREATE INDEX IF NOT EXISTS idx_podcast_feed ON podcast_cache(feed_url);
                CREATE INDEX IF NOT EXISTS idx_episode_feed ON episode_cache(feed_url);
                CREATE INDEX IF NOT EXISTS idx_favourites_url ON favourites(url);
                CREATE INDEX IF NOT EXISTS idx_radio_cache_parent ON radio_cache(parent_id);
                CREATE INDEX IF NOT EXISTS idx_youtube_cache_url ON youtube_cache(channel_url);
                -- V0.05B9n3e5g2RF8A10: 播放列表持久化表
                CREATE TABLE IF NOT EXISTS playlist (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    title TEXT NOT NULL,
                    url TEXT NOT NULL,
                    node_path TEXT,
                    duration INTEGER DEFAULT 0,
                    is_video INTEGER DEFAULT 0,
                    sort_order INTEGER DEFAULT 0,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
                CREATE INDEX IF NOT EXISTS idx_playlist_order ON playlist(sort_order);
            )";
            
            char* err_msg = nullptr;
            if (sqlite3_exec(db_, create_tables, nullptr, nullptr, &err_msg) != SQLITE_OK) {
                LOG(fmt::format("[DB] Create tables error: {}", err_msg));
                sqlite3_free(err_msg);
                return false;
            }
            
            initialized_ = true;
            LOG("[DB] Database initialized successfully");
            
            //-d: 自动清理过期历史记录
            cleanup_old_history();
            
            return true;
        }
        
        ~DatabaseManager() {
            if (db_) sqlite3_close(db_);
        }
        
        //清理过期历史记录（使用INI配置）
        void cleanup_old_history() {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            // 从INI读取配置
            int max_days = IniConfig::instance().get_history_max_days();
            int max_records = IniConfig::instance().get_history_max_records();
            
            // 按天数清理（如果配置>0）
            if (max_days > 0) {
                std::string sql_days = fmt::format(
                    "DELETE FROM history WHERE timestamp < datetime('now', '-{} days');",
                    max_days
                );
                exec_sql(sql_days);
            }
            
            // 按条数清理（如果配置>0）
            if (max_records > 0) {
                std::string sql_limit = fmt::format(
                    "DELETE FROM history WHERE id NOT IN "
                    "(SELECT id FROM history ORDER BY timestamp DESC LIMIT {});",
                    max_records
                );
                exec_sql(sql_limit);
            }
            
            LOG(fmt::format("[DB] History cleanup: max {} days, {} records", 
                           max_days, max_records));
        }
        
        // 进度管理
        void save_progress(const std::string& url, double position, bool completed) {
            if (!is_ready()) return;  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::string sql = fmt::format(
                "INSERT OR REPLACE INTO progress (url, position, completed, last_played) "
                "VALUES ('{}', {}, {}, datetime('now'));",
                escape_sql(url), position, completed ? 1 : 0
            );
            exec_sql(sql);
        }
        
        std::pair<double, bool> get_progress(const std::string& url) {
            if (!is_ready()) return {0.0, false};  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::string sql = fmt::format(
                "SELECT position, completed FROM progress WHERE url = '{}';", escape_sql(url)
            );
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                return {0.0, false};
            }
            
            double pos = 0.0;
            bool completed = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                pos = sqlite3_column_double(stmt, 0);
                completed = sqlite3_column_int(stmt, 1) != 0;
            }
            sqlite3_finalize(stmt);
            return {pos, completed};
        }
        
        // 历史记录
        void add_history(const std::string& url, const std::string& title, int duration) {
            if (!is_ready()) return;  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::string sql = fmt::format(
                "INSERT INTO history (url, title, duration, timestamp) "
                "VALUES ('{}', '{}', {}, datetime('now'));",
                escape_sql(url), escape_sql(title), duration
            );
            exec_sql(sql);
        }
        
        std::vector<std::tuple<std::string, std::string, std::string>> get_history(int limit = 50) {
            if (!is_ready()) return {};  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::vector<std::tuple<std::string, std::string, std::string>> result;
            std::string sql = fmt::format(
                "SELECT url, title, timestamp FROM history ORDER BY timestamp DESC LIMIT {};", limit
            );
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    result.push_back({
                        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))
                    });
                }
                sqlite3_finalize(stmt);
            }
            return result;
        }
        
        // 统计数据
        void increment_stat(const std::string& key, int64_t delta) {
            if (!is_ready()) return;  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::string sql = fmt::format(
                "INSERT INTO stats (key, value) VALUES ('{}', '{}') "
                "ON CONFLICT(key) DO UPDATE SET value = CAST(CAST(value AS INTEGER) + {} AS TEXT);",
                escape_sql(key), delta, delta
            );
            exec_sql(sql);
        }
        
        int64_t get_stat(const std::string& key, int64_t default_val = 0) {
            if (!is_ready()) return default_val;  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::string sql = fmt::format("SELECT value FROM stats WHERE key = '{}';", escape_sql(key));
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    sqlite3_finalize(stmt);
                    try { return std::stoll(val ? val : "0"); } catch (...) { return default_val; }
                }
                sqlite3_finalize(stmt);
            }
            return default_val;
        }
        
        // 缓存数据
        void cache_url_data(const std::string& url, const std::string& json_data) {
            if (!is_ready()) return;  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::string sql = fmt::format(
                "INSERT OR REPLACE INTO cache (url, data_json, timestamp) "
                "VALUES ('{}', '{}', strftime('%s', 'now'));",
                escape_sql(url), escape_sql(json_data)
            );
            exec_sql(sql);
        }
        
        std::string get_cached_data(const std::string& url) {
            if (!is_ready()) return "";  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::string sql = fmt::format("SELECT data_json FROM cache WHERE url = '{}';", escape_sql(url));
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    std::string result = data ? data : "";
                    sqlite3_finalize(stmt);
                    return result;
                }
                sqlite3_finalize(stmt);
            }
            return "";
        }
        
        // 节点保存/加载
        void save_node(const std::string& url, const std::string& title, int type, const std::string& json_data) {
            if (!is_ready()) return;  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            std::string sql = fmt::format(
                "INSERT OR REPLACE INTO nodes (url, title, type, parent_id, data_json) "
                "VALUES ('{}', '{}', {}, 0, '{}');",
                escape_sql(url), escape_sql(title), type, escape_sql(json_data)
            );
            exec_sql(sql);
        }
        
        void purge_all() {
            if (!is_ready()) return;  // V0.03a: 空指针检查
            std::lock_guard<std::mutex> lock(mtx_);
            //添加新表的清理
            sqlite3_exec(db_, "DELETE FROM nodes; DELETE FROM progress; DELETE FROM history; DELETE FROM stats; DELETE FROM cache; DELETE FROM search_cache; DELETE FROM player_state; DELETE FROM podcast_cache; DELETE FROM episode_cache; DELETE FROM favourites; DELETE FROM radio_cache; DELETE FROM youtube_cache; DELETE FROM url_cache;", 
                        nullptr, nullptr, nullptr);
        }
        
        //搜索缓存管理（使用INI配置）
        void save_search_cache(const std::string& query, const std::string& region, const std::string& result_json) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            //从INI读取最大缓存数
            int max_cache = IniConfig::instance().get_search_cache_max();
            
            //先检查是否存在相同query和region的记录
            std::string check_sql = fmt::format(
                "SELECT id FROM search_cache WHERE query='{}' AND region='{}' LIMIT 1;",
                escape_sql(query), escape_sql(region)
            );
            
            sqlite3_stmt* stmt;
            bool exists = false;
            if (sqlite3_prepare_v2(db_, check_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    exists = true;
                }
                sqlite3_finalize(stmt);
            }
            
            if (exists) {
                //更新现有记录的时间戳和结果
                std::string update_sql = fmt::format(
                    "UPDATE search_cache SET result_json='{}', timestamp=CURRENT_TIMESTAMP "
                    "WHERE query='{}' AND region='{}';",
                    escape_sql(result_json), escape_sql(query), escape_sql(region)
                );
                exec_sql(update_sql);
            } else {
                // 新增记录
                std::string sql = fmt::format(
                    "INSERT INTO search_cache (query, region, result_json) VALUES ('{}', '{}', '{}');",
                    escape_sql(query), escape_sql(region), escape_sql(result_json)
                );
                exec_sql(sql);
            }
            
            //只按数量清理，不按时间清理
            std::string cleanup = fmt::format(
                "DELETE FROM search_cache WHERE id NOT IN "
                "(SELECT id FROM search_cache ORDER BY timestamp DESC LIMIT {});",
                max_cache
            );
            exec_sql(cleanup);
        }
        
        //加载所有搜索历史（用于Online模式显示）
        struct SearchHistoryItem {
            int id;
            std::string query;
            std::string region;
            std::string timestamp;
            int result_count;
        };
        
        std::vector<SearchHistoryItem> load_all_search_history() {
            std::vector<SearchHistoryItem> history;
            if (!is_ready()) return history;
            std::lock_guard<std::mutex> lock(mtx_);
            
            //按时间倒序，最新搜索在最上面
            const char* sql = "SELECT id, query, region, timestamp, "
                              "(SELECT COUNT(*) FROM podcast_cache) as dummy "
                              "FROM search_cache ORDER BY timestamp DESC;";
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    SearchHistoryItem item;
                    item.id = sqlite3_column_int(stmt, 0);
                    
                    const char* q = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    item.query = q ? q : "";
                    
                    const char* r = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                    item.region = r ? r : "US";
                    
                    const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                    item.timestamp = t ? t : "";
                    
                    // 计算结果数量（需要解析JSON）
                    std::string json_sql = fmt::format(
                        "SELECT result_json FROM search_cache WHERE id={};", item.id
                    );
                    sqlite3_stmt* json_stmt;
                    if (sqlite3_prepare_v2(db_, json_sql.c_str(), -1, &json_stmt, nullptr) == SQLITE_OK) {
                        if (sqlite3_step(json_stmt) == SQLITE_ROW) {
                            const char* json_data = reinterpret_cast<const char*>(sqlite3_column_text(json_stmt, 0));
                            if (json_data) {
                                try {
                                    json j = json::parse(json_data);
                                    if (j.contains("results") && j["results"].is_array()) {
                                        item.result_count = j["results"].size();
                                    }
                                } catch (...) {
                                    item.result_count = 0;
                                }
                            }
                        }
                        sqlite3_finalize(json_stmt);
                    }
                    
                    history.push_back(item);
                }
                sqlite3_finalize(stmt);
            }
            return history;
        }
        
        //获取单个搜索缓存的完整数据
        std::string load_search_cache(const std::string& query, const std::string& region) {
            if (!is_ready()) return "";
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "SELECT result_json FROM search_cache WHERE query='{}' AND region='{}' ORDER BY timestamp DESC LIMIT 1;",
                escape_sql(query), escape_sql(region)
            );
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    std::string result = data ? data : "";
                    sqlite3_finalize(stmt);
                    return result;
                }
                sqlite3_finalize(stmt);
            }
            return "";
        }
        
        //收藏管理 - 保存收藏项
        void save_favourite(const std::string& title, const std::string& url, int type,
                           bool is_youtube = false, const std::string& channel_name = "",
                           const std::string& source_type = "", const std::string& data_json = "") {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "INSERT OR REPLACE INTO favourites (title, url, type, is_youtube, channel_name, source_type, data_json) "
                "VALUES ('{}', '{}', {}, {}, '{}', '{}', '{}');",
                escape_sql(title), escape_sql(url), type, is_youtube ? 1 : 0,
                escape_sql(channel_name), escape_sql(source_type), escape_sql(data_json)
            );
            exec_sql(sql);
        }
        
        //收藏管理 - 加载所有收藏
        std::vector<std::tuple<std::string, std::string, int, bool, std::string, std::string, std::string>> load_favourites() {
            std::vector<std::tuple<std::string, std::string, int, bool, std::string, std::string, std::string>> favs;
            if (!is_ready()) return favs;
            std::lock_guard<std::mutex> lock(mtx_);
            
            const char* sql = "SELECT title, url, type, is_youtube, channel_name, source_type, data_json FROM favourites ORDER BY created_at DESC;";
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    int type = sqlite3_column_int(stmt, 2);
                    bool is_youtube = sqlite3_column_int(stmt, 3) != 0;
                    const char* channel_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                    const char* source_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                    const char* data_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
                    
                    favs.push_back({
                        title ? title : "",
                        url ? url : "",
                        type,
                        is_youtube,
                        channel_name ? channel_name : "",
                        source_type ? source_type : "",
                        data_json ? data_json : ""
                    });
                }
                sqlite3_finalize(stmt);
            }
            return favs;
        }
        
        //收藏管理 - 删除收藏项
        void delete_favourite(const std::string& url) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "DELETE FROM favourites WHERE url='{}';",
                escape_sql(url)
            );
            exec_sql(sql);
        }
        
        //清空nodes表（用于保存订阅前清空旧数据）
        void clear_nodes() {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            exec_sql("DELETE FROM nodes;");
            LOG("[DB] Cleared nodes table");
        }
        
        //清空favourites表（用于保存收藏前清空旧数据）
        void clear_favourites() {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            exec_sql("DELETE FROM favourites;");
            LOG("[DB] Cleared favourites table");
        }
        
        //加载所有订阅节点
        std::vector<std::tuple<std::string, std::string, int, std::string>> load_nodes() {
            std::vector<std::tuple<std::string, std::string, int, std::string>> nodes;
            if (!is_ready()) return nodes;
            std::lock_guard<std::mutex> lock(mtx_);
            
            const char* sql = "SELECT title, url, type, data_json FROM nodes WHERE parent_id = 0 ORDER BY updated_at DESC;";
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    int type = sqlite3_column_int(stmt, 2);
                    const char* data_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                    
                    nodes.push_back({
                        title ? title : "",
                        url ? url : "",
                        type,
                        data_json ? data_json : ""
                    });
                }
                sqlite3_finalize(stmt);
            }
            return nodes;
        }
        
        //收藏管理 - 检查是否已收藏
        bool is_favourite(const std::string& url) {
            if (!is_ready()) return false;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "SELECT 1 FROM favourites WHERE url='{}' LIMIT 1;",
                escape_sql(url)
            );
            
            sqlite3_stmt* stmt;
            bool result = false;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    result = true;
                }
                sqlite3_finalize(stmt);
            }
            return result;
        }
        
        //删除指定搜索记录
        void delete_search_history(const std::string& query, const std::string& region) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "DELETE FROM search_cache WHERE query='{}' AND region='{}';",
                escape_sql(query), escape_sql(region)
            );
            exec_sql(sql);
            LOG(fmt::format("[DB] Deleted search history: {} [{}]", query, region));
        }
        
        //删除指定播放历史记录
        void delete_history(const std::string& url) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "DELETE FROM history WHERE url='{}';",
                escape_sql(url)
            );
            exec_sql(sql);
            LOG(fmt::format("[DB] Deleted history: {}", url));
        }
        
        //删除播客缓存
        void delete_podcast_cache(const std::string& feed_url) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "DELETE FROM podcast_cache WHERE feed_url='{}';",
                escape_sql(feed_url)
            );
            exec_sql(sql);
            LOG(fmt::format("[DB] Deleted podcast cache: {}", feed_url));
        }
        
        //删除节目缓存（根据feed_url）
        void delete_episode_cache_by_feed(const std::string& feed_url) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "DELETE FROM episode_cache WHERE feed_url='{}';",
                escape_sql(feed_url)
            );
            exec_sql(sql);
            LOG(fmt::format("[DB] Deleted episode cache for feed: {}", feed_url));
        }
        
        //播客Feed缓存管理
        void save_podcast_cache(const std::string& feed_url, const std::string& title, 
                                const std::string& artist, const std::string& genre,
                                int track_count, const std::string& artwork_url,
                                int collection_id, const std::string& data_json) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "INSERT OR REPLACE INTO podcast_cache (feed_url, title, artist, genre, track_count, artwork_url, collection_id, data_json) "
                "VALUES ('{}', '{}', '{}', '{}', {}, '{}', {}, '{}');",
                escape_sql(feed_url), escape_sql(title), escape_sql(artist), escape_sql(genre),
                track_count, escape_sql(artwork_url), collection_id, escape_sql(data_json)
            );
            exec_sql(sql);
        }
        
        //检查播客是否已缓存
        bool is_podcast_cached(const std::string& feed_url) {
            if (!is_ready()) return false;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "SELECT 1 FROM podcast_cache WHERE feed_url='{}' LIMIT 1;",
                escape_sql(feed_url)
            );
            
            sqlite3_stmt* stmt;
            bool found = false;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                }
                sqlite3_finalize(stmt);
            }
            return found;
        }
        
        //加载播客缓存数据
        std::string load_podcast_cache(const std::string& feed_url) {
            if (!is_ready()) return "";
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "SELECT data_json FROM podcast_cache WHERE feed_url='{}' LIMIT 1;",
                escape_sql(feed_url)
            );
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    std::string result = data ? data : "";
                    sqlite3_finalize(stmt);
                    return result;
                }
                sqlite3_finalize(stmt);
            }
            return "";
        }
        
        //保存节目列表缓存
        void save_episode_cache(const std::string& feed_url, const std::string& episodes_json) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            // 先删除该feed的旧缓存
            std::string del_sql = fmt::format(
                "DELETE FROM episode_cache WHERE feed_url='{}';",
                escape_sql(feed_url)
            );
            exec_sql(del_sql);
            
            // 解析JSON并逐条保存
            try {
                json j = json::parse(episodes_json);
                if (j.is_array()) {
                    for (const auto& ep : j) {
                        std::string ep_url = ep.value("url", "");
                        std::string ep_title = ep.value("title", "");
                        int ep_duration = ep.value("duration", 0);
                        std::string ep_pub_date = ep.value("pubDate", "");
                        
                        std::string sql = fmt::format(
                            "INSERT INTO episode_cache (feed_url, episode_url, title, duration, pub_date, data_json) "
                            "VALUES ('{}', '{}', '{}', {}, '{}', '{}');",
                            escape_sql(feed_url), escape_sql(ep_url), escape_sql(ep_title),
                            ep_duration, escape_sql(ep_pub_date), escape_sql(ep.dump())
                        );
                        exec_sql(sql);
                    }
                }
            } catch (...) {}
        }
        
        //检查节目列表是否已缓存
        bool is_episode_cached(const std::string& feed_url) {
            if (!is_ready()) return false;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "SELECT COUNT(*) FROM episode_cache WHERE feed_url='{}';",
                escape_sql(feed_url)
            );
            
            sqlite3_stmt* stmt;
            int count = 0;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    count = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
            return count > 0;
        }
        
        //加载节目列表缓存
        std::string load_episode_cache(const std::string& feed_url) {
            if (!is_ready()) return "";
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "SELECT data_json FROM episode_cache WHERE feed_url='{}' ORDER BY timestamp DESC;",
                escape_sql(feed_url)
            );
            
            json episodes = json::array();
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    if (data) {
                        try {
                            episodes.push_back(json::parse(data));
                        } catch (...) {}
                    }
                }
                sqlite3_finalize(stmt);
            }
            return episodes.dump();
        }
        
        //加载节目列表缓存（结构化返回）
        // 用于收藏引用模式展开时从数据库缓存加载子节点
        std::vector<std::tuple<std::string, std::string, int, std::string>> load_episodes_from_cache(const std::string& feed_url) {
            std::vector<std::tuple<std::string, std::string, int, std::string>> episodes;
            if (!is_ready()) return episodes;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "SELECT episode_url, title, duration, data_json FROM episode_cache WHERE feed_url='{}' ORDER BY timestamp DESC;",
                escape_sql(feed_url)
            );
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* ep_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const char* ep_title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    int ep_duration = sqlite3_column_int(stmt, 2);
                    const char* ep_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                    
                    episodes.push_back({
                        ep_url ? ep_url : "",
                        ep_title ? ep_title : "",
                        ep_duration,
                        ep_data ? ep_data : ""
                    });
                }
                sqlite3_finalize(stmt);
            }
            return episodes;
        }
        
        //扩展播放器状态保存
        void save_player_state(int volume, double speed, bool paused, const std::string& url = "", double position = 0,
                                bool scroll_mode = false, bool show_tree_lines = true, 
                                const std::string& current_title = "", int current_mode = 0) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            std::string sql = fmt::format(
                "INSERT OR REPLACE INTO player_state (id, volume, speed, paused, current_url, position, scroll_mode, show_tree_lines, current_title, current_mode) "
                "VALUES (1, {}, {}, {}, '{}', {}, {}, {}, '{}', {});",
                volume, speed, paused ? 1 : 0, escape_sql(url), position,
                scroll_mode ? 1 : 0, show_tree_lines ? 1 : 0,
                escape_sql(current_title), current_mode
            );
            exec_sql(sql);
        }
        
        //播放器状态加载
        //扩展状态数据
        struct PlayerStateData {
            int volume = 100;
            double speed = 1.0;
            bool paused = false;
            std::string current_url;
            double position = 0;
            //新增字段
            bool scroll_mode = false;
            bool show_tree_lines = true;
            std::string current_title;
            int current_mode = 0;
        };
        
        PlayerStateData load_player_state() {
            PlayerStateData state;
            if (!is_ready()) return state;
            std::lock_guard<std::mutex> lock(mtx_);
            
            sqlite3_stmt* stmt;
            const char* sql = "SELECT volume, speed, paused, current_url, position, scroll_mode, show_tree_lines, current_title, current_mode FROM player_state WHERE id=1;";
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    state.volume = sqlite3_column_int(stmt, 0);
                    state.speed = sqlite3_column_double(stmt, 1);
                    state.paused = sqlite3_column_int(stmt, 2) != 0;
                    const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                    state.current_url = url ? url : "";
                    state.position = sqlite3_column_double(stmt, 4);
                    //加载新字段
                    state.scroll_mode = sqlite3_column_int(stmt, 5) != 0;
                    state.show_tree_lines = sqlite3_column_int(stmt, 6) != 0;
                    const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
                    state.current_title = title ? title : "";
                    state.current_mode = sqlite3_column_int(stmt, 8);
                }
                sqlite3_finalize(stmt);
            }
            return state;
        }

        // ═════════════════════════════════════════════════════════════════════════
        //播放列表持久化操作
        // ═════════════════════════════════════════════════════════════════════════

        // 保存播放列表项
        void save_playlist_item(const SavedPlaylistItem& item) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);

            std::string sql = fmt::format(
                "INSERT INTO playlist (title, url, node_path, duration, is_video, sort_order) "
                "VALUES ('{}', '{}', '{}', {}, {}, {});",
                escape_sql(item.title), escape_sql(item.url), escape_sql(item.node_path),
                item.duration, item.is_video ? 1 : 0, item.sort_order
            );
            exec_sql(sql);
        }

        // 加载所有播放列表项
        std::vector<SavedPlaylistItem> load_playlist() {
            std::vector<SavedPlaylistItem> items;
            if (!is_ready()) return items;
            std::lock_guard<std::mutex> lock(mtx_);

            const char* sql = "SELECT id, title, url, node_path, duration, is_video, sort_order "
                              "FROM playlist ORDER BY sort_order;";

            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    SavedPlaylistItem item;
                    item.id = sqlite3_column_int(stmt, 0);
                    const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    item.title = title ? title : "";
                    const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                    item.url = url ? url : "";
                    const char* node_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                    item.node_path = node_path ? node_path : "";
                    item.duration = sqlite3_column_int(stmt, 4);
                    item.is_video = sqlite3_column_int(stmt, 5) != 0;
                    item.sort_order = sqlite3_column_int(stmt, 6);
                    items.push_back(item);
                }
                sqlite3_finalize(stmt);
            }
            return items;
        }

        // 删除播放列表项
        void delete_playlist_item(int id) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);

            std::string sql = fmt::format("DELETE FROM playlist WHERE id={};", id);
            exec_sql(sql);
        }

        // 清空播放列表
        void clear_playlist_table() {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);

            exec_sql("DELETE FROM playlist;");
            LOG("[DB] Playlist cleared");
        }

        // 更新播放列表项排序
        void update_playlist_order(int id, int new_order) {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);

            std::string sql = fmt::format(
                "UPDATE playlist SET sort_order={} WHERE id={};", new_order, id
            );
            exec_sql(sql);
        }

        // 重新排序所有播放列表项（用于移动操作后）
        void reorder_playlist() {
            if (!is_ready()) return;
            std::lock_guard<std::mutex> lock(mtx_);

            // 获取所有项并重新分配排序号
            const char* sql = "SELECT id FROM playlist ORDER BY sort_order;";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                int order = 0;
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int id = sqlite3_column_int(stmt, 0);
                    std::string update_sql = fmt::format(
                        "UPDATE playlist SET sort_order={} WHERE id={};", order++, id
                    );
                    exec_sql(update_sql);
                }
                sqlite3_finalize(stmt);
            }
        }

        //保存RADIO树到数据库
        void save_radio_cache(const TreeNodePtr& root) {
            if (!is_ready() || !root) return;
            std::lock_guard<std::mutex> lock(mtx_);
            
            // 清空旧数据
            exec_sql("DELETE FROM radio_cache;");
            
            // 递归保存节点
            int order = 0;
            save_radio_node_recursive(root, 0, order);
        }
        
        //从数据库加载RADIO树
        TreeNodePtr load_radio_cache() {
            if (!is_ready()) return nullptr;
            std::lock_guard<std::mutex> lock(mtx_);
            
            // 创建根节点
            auto root = std::make_shared<TreeNode>();
            root->title = "Radio";
            root->type = NodeType::FOLDER;
            root->expanded = true;
            
            // 加载顶层节点（parent_id=0）
            load_radio_node_recursive(root, 0);
            
            return root;
        }
        
        //公开的is_ready方法供外部调用
        bool is_ready() const { return initialized_ && db_ != nullptr; }
        
    private:
        //递归保存RADIO节点
        void save_radio_node_recursive(const TreeNodePtr& node, int parent_id, int& order) {
            if (!node) return;
            
            for (const auto& child : node->children) {
                // 插入当前节点
                std::string sql = fmt::format(
                    "INSERT INTO radio_cache (parent_id, title, url, type, expanded, children_loaded, "
                    "is_youtube, channel_name, is_cached, is_downloaded, local_file, sort_order) "
                    "VALUES ({}, '{}', '{}', {}, {}, {}, {}, '{}', {}, {}, '{}', {});",
                    parent_id, escape_sql(child->title), escape_sql(child->url), (int)child->type,
                    child->expanded ? 1 : 0, child->children_loaded ? 1 : 0,
                    child->is_youtube ? 1 : 0, escape_sql(child->channel_name),
                    child->is_cached ? 1 : 0, child->is_downloaded ? 1 : 0,
                    escape_sql(child->local_file), order++
                );
                exec_sql(sql);
                
                // 获取插入的ID并递归保存子节点
                int64_t new_id = sqlite3_last_insert_rowid(db_);
                save_radio_node_recursive(child, (int)new_id, order);
            }
        }
        
        //递归加载RADIO节点
        void load_radio_node_recursive(const TreeNodePtr& parent, int parent_id) {
            std::string sql = fmt::format(
                "SELECT id, title, url, type, expanded, children_loaded, is_youtube, channel_name, "
                "is_cached, is_downloaded, local_file FROM radio_cache WHERE parent_id={} ORDER BY sort_order;",
                parent_id
            );
            
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return;
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto node = std::make_shared<TreeNode>();
                int node_id = sqlite3_column_int(stmt, 0);
                
                const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                int type = sqlite3_column_int(stmt, 3);
                bool expanded = sqlite3_column_int(stmt, 4) != 0;
                bool children_loaded = sqlite3_column_int(stmt, 5) != 0;
                bool is_youtube = sqlite3_column_int(stmt, 6) != 0;
                const char* channel_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
                bool is_cached = sqlite3_column_int(stmt, 8) != 0;
                bool is_downloaded = sqlite3_column_int(stmt, 9) != 0;
                const char* local_file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
                
                node->title = title ? title : "";
                node->url = url ? url : "";
                node->type = (NodeType)type;
                node->expanded = expanded;
                node->children_loaded = children_loaded;
                node->is_youtube = is_youtube;
                node->channel_name = channel_name ? channel_name : "";
                node->is_cached = is_cached;
                node->is_downloaded = is_downloaded;
                node->local_file = local_file ? local_file : "";
                
                // 递归加载子节点
                load_radio_node_recursive(node, node_id);
                
                node->parent = parent;  //设置父节点指针
                parent->children.push_back(node);
            }
            sqlite3_finalize(stmt);
        }
        
        DatabaseManager() : initialized_(false), db_(nullptr) {}
        bool initialized_ = false;  //单例初始化标志
        sqlite3* db_ = nullptr;
        std::mutex mtx_;
        
        std::string escape_sql(const std::string& s) {
            std::string result;
            result.reserve(s.size() * 2);
            for (char c : s) {
                if (c == '\'') result += "''";
                else result += c;
            }
            return result;
        }
        
        bool exec_sql(const std::string& sql) {
            char* err_msg = nullptr;
            int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
            if (rc != SQLITE_OK) {
                LOG(fmt::format("[DB] SQL error: {}", err_msg ? err_msg : "unknown"));
                if (err_msg) sqlite3_free(err_msg);
                return false;
            }
            return true;
        }
    };

    // =========================================================
    // 缓存管理
    // =========================================================
    class CacheManager {
    public:
        static CacheManager& instance() { static CacheManager cm; return cm; }
        
        void mark_cached(const std::string& url) { 
            cached_.insert(url); 
            save();
        }
        bool is_cached(const std::string& url) { return cached_.count(url) > 0; }
        
        void mark_downloaded(const std::string& url, const std::string& local_file = "") { 
            downloaded_.insert(url); 
            if (!local_file.empty()) local_files_[url] = local_file;
            save();
        }
        bool is_downloaded(const std::string& url) { return downloaded_.count(url) > 0; }
        
        std::string get_local_file(const std::string& url) {
            if (local_files_.count(url)) return local_files_[url];
            return "";
        }
        
        void set_local_file(const std::string& url, const std::string& path) {
            local_files_[url] = path;
            mark_downloaded(url, path);
        }
        
        void clear_cache(const std::string& url) {
            cached_.erase(url);
            save();
        }
        
        void clear_download(const std::string& url) {
            downloaded_.erase(url);
            if (local_files_.count(url)) {
                std::string path = local_files_[url];
                local_files_.erase(url);
                try {
                    if (fs::exists(path)) fs::remove(path);
                } catch (...) {}
            }
            save();
        }
        
        bool is_cached_or_downloaded(const std::string& url) {
            return cached_.count(url) > 0 || downloaded_.count(url) > 0;
        }
        
        void save() {
            const char* home = std::getenv("HOME");
            if (!home) return;
            
            std::string path = std::string(home) + DATA_DIR + "/cached_urls.json";
            fs::create_directories(fs::path(path).parent_path());
            
            json j;
            j["cached"] = json::array();
            j["downloaded"] = json::array();
            j["local_files"] = json::object();
            for (const auto& url : cached_) j["cached"].push_back(url);
            for (const auto& url : downloaded_) j["downloaded"].push_back(url);
            for (const auto& [url, path] : local_files_) j["local_files"][url] = path;
            
            std::ofstream f(path);
            if (f.is_open()) f << j.dump(2);
        }
        
        void load() {
            const char* home = std::getenv("HOME");
            if (!home) return;
            
            std::string path = std::string(home) + DATA_DIR + "/cached_urls.json";
            if (!fs::exists(path)) return;
            
            try {
                std::ifstream f(path);
                json j; f >> j;
                if (j.contains("cached")) {
                    for (const auto& url : j["cached"]) {
                        if (url.is_string()) cached_.insert(url.get<std::string>());
                    }
                }
                if (j.contains("downloaded")) {
                    for (const auto& url : j["downloaded"]) {
                        if (url.is_string()) downloaded_.insert(url.get<std::string>());
                    }
                }
                if (j.contains("local_files")) {
                    for (auto& [url, path] : j["local_files"].items()) {
                        if (path.is_string()) local_files_[url] = path.get<std::string>();
                    }
                }
            } catch (...) {}
        }
        
    private:
        std::set<std::string> cached_;
        std::set<std::string> downloaded_;
        std::map<std::string, std::string> local_files_;
    };

    // =========================================================
    //iTunes搜索功能
    // =========================================================
    class ITunesSearch {
    public:
        static ITunesSearch& instance() { static ITunesSearch is; return is; }
        
        // 支持的地区列表
        static std::vector<std::string> get_regions() {
            return {"US", "CN", "TW", "JP", "UK", "DE", "FR", "KR", "AU"};
        }
        
        static std::string get_region_name(const std::string& region) {
            static std::map<std::string, std::string> names = {
                {"US", "美国"}, {"CN", "中国"}, {"TW", "台湾"}, {"JP", "日本"},
                {"UK", "英国"}, {"DE", "德国"}, {"FR", "法国"}, {"KR", "韩国"}, {"AU", "澳大利亚"}
            };
            return names.count(region) ? names[region] : region;
        }
        
        //搜索播客（带缓存标记）
        std::vector<TreeNodePtr> search(const std::string& query, const std::string& region = "US", int limit = 50) {
            std::vector<TreeNodePtr> results;
            
            // 先检查缓存
            std::string cached = DatabaseManager::instance().load_search_cache(query, region);
            if (!cached.empty()) {
                try {
                    json j = json::parse(cached);
                    if (j.contains("results") && j["results"].is_array()) {
                        for (const auto& item : j["results"]) {
                            auto node = parse_result(item);
                            if (node) {
                                //检查是否已在podcast_cache中
                                node->is_db_cached = DatabaseManager::instance().is_podcast_cached(node->url);
                                results.push_back(node);
                            }
                        }
                    }
                    EVENT_LOG(fmt::format("[iTunes] Loaded from cache: {} results for '{}'", results.size(), query));
                    return results;
                } catch (...) {}
            }
            
            // 网络请求
            std::string url = fmt::format(
                "https://itunes.apple.com/search?term={}&media=podcast&country={}&limit={}",
                Utils::url_encode(query), region, limit
            );
            
            std::string response = fetch(url);
            if (response.empty()) {
                EVENT_LOG(fmt::format("[iTunes] Search failed for '{}'", query));
                return results;
            }
            
            // 保存到搜索缓存
            DatabaseManager::instance().save_search_cache(query, region, response);
            
            // 解析结果并保存到podcast_cache
            try {
                json j = json::parse(response);
                if (j.contains("results") && j["results"].is_array()) {
                    for (const auto& item : j["results"]) {
                        auto node = parse_result(item);
                        if (node) {
                            //保存到podcast_cache
                            save_podcast_to_cache(item);
                            // 检查是否已缓存
                            node->is_db_cached = DatabaseManager::instance().is_podcast_cached(node->url);
                            results.push_back(node);
                        }
                    }
                }
            } catch (const std::exception& e) {
                LOG(fmt::format("[iTunes] Parse error: {}", e.what()));
            }
            
            EVENT_LOG(fmt::format("[iTunes] Search '{}': {} results", query, results.size()));
            return results;
        }
        
        //公开parse_result方法供外部调用
        TreeNodePtr parse_result(const json& item) {
            auto node = std::make_shared<TreeNode>();
            node->type = NodeType::PODCAST_FEED;
            
            // 从iTunes结果提取信息
            if (item.contains("collectionName")) {
                node->title = item["collectionName"].get<std::string>();
            } else if (item.contains("trackName")) {
                node->title = item["trackName"].get<std::string>();
            } else {
                return nullptr;
            }
            
            if (item.contains("feedUrl")) {
                node->url = item["feedUrl"].get<std::string>();
            } else {
                return nullptr;  // 没有feed URL的跳过
            }
            
            // 添加子文本显示更多信息
            std::string subtext;
            if (item.contains("artistName")) {
                subtext = item["artistName"].get<std::string>();
            }
            if (item.contains("trackCount")) {
                if (!subtext.empty()) subtext += " · ";
                subtext += std::to_string(item["trackCount"].get<int>()) + " episodes";
            }
            if (item.contains("primaryGenreName")) {
                if (!subtext.empty()) subtext += " · ";
                subtext += item["primaryGenreName"].get<std::string>();
            }
            node->subtext = subtext;
            
            return node;
        }
        
    private:
        ITunesSearch() {}
        
        //保存播客信息到缓存
        void save_podcast_to_cache(const json& item) {
            if (!item.contains("feedUrl")) return;
            
            std::string feed_url = item["feedUrl"].get<std::string>();
            std::string title = item.value("collectionName", "");
            std::string artist = item.value("artistName", "");
            std::string genre = item.value("primaryGenreName", "");
            int track_count = item.value("trackCount", 0);
            std::string artwork_url = item.value("artworkUrl600", item.value("artworkUrl100", ""));
            int collection_id = item.value("collectionId", 0);
            
            DatabaseManager::instance().save_podcast_cache(
                feed_url, title, artist, genre, track_count, artwork_url, collection_id, item.dump()
            );
        }
        
        std::string fetch(const std::string& url) {
            CURL* curl = curl_easy_init();
            if (!curl) return "";
            
            std::string response;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, void* data) {
                ((std::string*)data)->append((char*)ptr, size * nmemb);
                return size * nmemb;
            });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            
            return (res == CURLE_OK) ? response : "";
        }
    };
    
    //Online模式状态管理
    //搜索历史节点化管理
    class OnlineState {
    public:
        static OnlineState& instance() { static OnlineState os; return os; }
        
        std::string current_region = "US";
        std::string last_query;
        std::vector<TreeNodePtr> search_results;
        TreeNodePtr online_root = std::make_shared<TreeNode>();
        bool history_loaded = false;  //标记历史是否已加载
        
        void set_region(const std::string& region) {
            current_region = region;
        }
        
        std::string get_next_region() {
            auto regions = ITunesSearch::get_regions();
            auto it = std::find(regions.begin(), regions.end(), current_region);
            if (it != regions.end() && ++it != regions.end()) {
                current_region = *it;
            } else {
                current_region = regions[0];
            }
            return current_region;
        }
        
        //从数据库加载搜索历史并构建节点树
        void load_search_history() {
            if (history_loaded) return;
            
            online_root->children.clear();
            auto history = DatabaseManager::instance().load_all_search_history();
            
            for (const auto& item : history) {
                auto node = create_search_node(item);
                if (node) {
                    node->parent = online_root;  //设置父节点指针
                    online_root->children.push_back(node);
                }
            }
            
            history_loaded = true;
            EVENT_LOG(fmt::format("[Online] Loaded {} search history items", history.size()));
        }
        
        //添加或更新搜索节点（搜索后调用）
        void add_or_update_search_node(const std::string& query, const std::string& region, 
                                       const std::vector<TreeNodePtr>& results) {
            // 查找是否已存在相同query和region的节点
            TreeNodePtr existing_node = nullptr;
            for (auto& child : online_root->children) {
                if (child->url == make_search_id(query, region)) {
                    existing_node = child;
                    break;
                }
            }
            
            if (existing_node) {
                //更新现有节点 - 移到最前面，更新子节点
                existing_node->children.clear();
                for (const auto& result : results) {
                    result->parent = existing_node;  //设置父节点指针
                    existing_node->children.push_back(result);
                }
                // 更新标题中的结果数量和时间
                update_search_node_title(existing_node, query, region, results.size());
                
                // 移动到最前面（按时间倒序）
                auto it = std::find(online_root->children.begin(), online_root->children.end(), existing_node);
                if (it != online_root->children.begin()) {
                    online_root->children.erase(it);
                    online_root->children.insert(online_root->children.begin(), existing_node);
                }
            } else {
                // 创建新节点并插入到最前面
                auto node = std::make_shared<TreeNode>();
                node->type = NodeType::FOLDER;
                node->url = make_search_id(query, region);
                update_search_node_title(node, query, region, results.size());
                
                for (const auto& result : results) {
                    result->parent = node;  //设置父节点指针
                    node->children.push_back(result);
                }
                
                node->parent = online_root;  //设置父节点指针
                online_root->children.insert(online_root->children.begin(), node);
            }
        }
        
        void clear_results() {
            search_results.clear();
            //不再清空online_root->children，保留历史
        }
        
        //强制重新加载历史
        void reload_history() {
            history_loaded = false;
            load_search_history();
        }
        
    private:
        OnlineState() {
            online_root->title = "Online Search";
            online_root->type = NodeType::FOLDER;
        }
        
        //创建搜索历史节点
        //格式改为 🔍 [时间][地区][数量(4位)]["关键词"]
        TreeNodePtr create_search_node(const DatabaseManager::SearchHistoryItem& item) {
            auto node = std::make_shared<TreeNode>();
            node->type = NodeType::FOLDER;
            node->url = make_search_id(item.query, item.region);
            
            std::string region_name = ITunesSearch::get_region_name(item.region);
            std::string time_str = format_timestamp(item.timestamp);
            
            node->title = fmt::format("🔍 [{}][{}][{:4d}][\"{}\"]", 
                time_str, region_name, item.result_count, item.query);
            
            // 子节点需要展开时从缓存加载
            node->children_loaded = false;
            
            return node;
        }
        
        //更新搜索节点标题
        //格式改为 🔍 [时间][地区][数量(4位)]["关键词"]
        void update_search_node_title(TreeNodePtr node, const std::string& query, 
                                      const std::string& region, int count) {
            std::string region_name = ITunesSearch::get_region_name(region);
            std::string time_str = get_current_time_str();
            
            node->title = fmt::format("🔍 [{}][{}][{:4d}][\"{}\"]", 
                time_str, region_name, count, query);
        }
        
        //生成搜索唯一标识
        std::string make_search_id(const std::string& query, const std::string& region) {
            return fmt::format("search:{}:{}", region, query);
        }
        
        //格式化数据库时间戳
        std::string format_timestamp(const std::string& timestamp) {
            // 输入格式: "2026-03-05 19:33:36"
            // 输出格式: "2026-03-05 19:33:36" (保持不变)
            if (timestamp.empty()) return "";
            
            // 尝试解析并格式化
            // SQLite CURRENT_TIMESTAMP 格式: YYYY-MM-DD HH:MM:SS
            return timestamp;
        }
        
        //获取当前时间字符串
        std::string get_current_time_str() {
            auto now = std::time(nullptr);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            return std::string(buf);
        }
    };

    // =========================================================
    // YouTube缓存
    // =========================================================
    struct YouTubeVideoInfo { std::string id, title, url; };
    struct YouTubeChannelCache { std::string channel_name; std::vector<YouTubeVideoInfo> videos; };

    class YouTubeCache {
    public:
        static YouTubeCache& instance() { static YouTubeCache yc; return yc; }
        
        //改用数据库存储，不再使用JSON文件
        void load() {
            // YouTube缓存现在从数据库youtube_cache表加载
            // 由DatabaseManager管理，此处为内存缓存初始化
            cache_.clear();
        }
        
        void save() {
            // 保存到数据库youtube_cache表
            // 由update()方法直接保存到数据库
        }
        
        bool has(const std::string& url) { 
            //从内存缓存检查，如果没有则检查数据库
            if (cache_.count(url) > 0) return true;
            return load_from_db(url);
        }
        
        YouTubeChannelCache get(const std::string& url) { 
            if (cache_.count(url)) return cache_[url];
            // 尝试从数据库加载
            if (load_from_db(url) && cache_.count(url)) {
                return cache_[url];
            }
            return YouTubeChannelCache(); 
        }
        
        void update(const std::string& url, const std::string& name, const std::vector<YouTubeVideoInfo>& videos) {
            cache_[url] = {name, videos};
            //保存到数据库
            save_to_db(url, name, videos);
        }
        
    private:
        std::map<std::string, YouTubeChannelCache> cache_;
        
        //从数据库加载YouTube缓存
        bool load_from_db(const std::string& channel_url) {
            if (!DatabaseManager::instance().is_ready()) return false;
            
            // 使用DatabaseManager的内部方法（需要添加）
            // 这里简化处理：直接查询youtube_cache表
            const char* home = std::getenv("HOME");
            if (!home) return false;
            
            std::string db_path = std::string(home) + DATA_DIR + "/podradio.db";
            sqlite3* db = nullptr;
            if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
                if (db) sqlite3_close(db);
                return false;
            }
            
            std::string sql = fmt::format(
                "SELECT channel_name, videos_json FROM youtube_cache WHERE channel_url='{}' LIMIT 1;",
                channel_url
            );
            
            sqlite3_stmt* stmt;
            bool found = false;
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const char* videos_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    
                    YouTubeChannelCache cache;
                    cache.channel_name = name ? name : "";
                    
                    if (videos_json) {
                        try {
                            json j = json::parse(videos_json);
                            if (j.is_array()) {
                                for (const auto& vid : j) {
                                    cache.videos.push_back({
                                        vid.value("id", ""),
                                        vid.value("title", ""),
                                        vid.value("url", "")
                                    });
                                }
                            }
                        } catch (...) {}
                    }
                    
                    cache_[channel_url] = cache;
                    found = true;
                }
                sqlite3_finalize(stmt);
            }
            sqlite3_close(db);
            return found;
        }
        
        //保存YouTube缓存到数据库
        void save_to_db(const std::string& channel_url, const std::string& channel_name, 
                        const std::vector<YouTubeVideoInfo>& videos) {
            if (!DatabaseManager::instance().is_ready()) return;
            
            const char* home = std::getenv("HOME");
            if (!home) return;
            
            std::string db_path = std::string(home) + DATA_DIR + "/podradio.db";
            sqlite3* db = nullptr;
            if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
                if (db) sqlite3_close(db);
                return;
            }
            
            // 构建videos JSON
            json videos_json = json::array();
            for (const auto& vi : videos) {
                videos_json.push_back({
                    {"id", vi.id},
                    {"title", vi.title},
                    {"url", vi.url}
                });
            }
            
            // 转义SQL字符串
            auto escape_sql = [](const std::string& s) {
                std::string result;
                for (char c : s) {
                    if (c == '\'') result += "''";
                    else result += c;
                }
                return result;
            };
            
            std::string sql = fmt::format(
                "INSERT OR REPLACE INTO youtube_cache (channel_url, channel_name, videos_json, updated_at) "
                "VALUES ('{}', '{}', '{}', CURRENT_TIMESTAMP);",
                escape_sql(channel_url), escape_sql(channel_name), escape_sql(videos_json.dump())
            );
            
            char* err_msg = nullptr;
            sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
            if (err_msg) sqlite3_free(err_msg);
            sqlite3_close(db);
        }
    };

    // =========================================================
    // 网络请求
    // =========================================================
    class Network {
    public:
        //增强SSL兼容性，修复部分HTTPS网站的连接问题
        static std::string fetch(const std::string& url, int timeout = DEFAULT_NETWORK_TIMEOUT_SEC) {
            CURL* curl = curl_easy_init();
            if (!curl) return "";
            std::string data;
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Accept: application/xml,application/xhtml+xml,text/xml;q=0.9,*/*;q=0.8");
            
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
            
            //完整的SSL跳过选项，解决各种SSL连接问题
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);      // 跳过证书验证
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);      // 跳过主机名验证
            curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);  // 使用TLS 1.0+兼容
            curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);     // 禁用ALPN（部分服务器兼容）
#if LIBCURL_VERSION_NUM < 0x075600  // CURLOPT_SSL_ENABLE_NPN deprecated since curl 7.86.0
            curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_NPN, 0L);      // 禁用NPN
#endif
            
            //设置连接超时和DNS缓存
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);     // 连接超时15秒
            curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 60L);  // DNS缓存60秒
            
            CURLcode res = curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            if (res != CURLE_OK) {
                EVENT_LOG(fmt::format("Network error: {} (URL: {})", curl_easy_strerror(res), url.substr(0, 60)));
            }
            return data;
        }
        
        static std::string lookup_apple_feed(const std::string& url) {
            std::regex id_regex("/id(\\d+)");
            std::smatch match;
            if (!std::regex_search(url, match, id_regex)) return "";
            std::string response = fetch("https://itunes.apple.com/lookup?id=" + match[1].str() + "&entity=podcast");
            if (response.empty()) return "";
            try {
                json j = json::parse(response);
                if (j.contains("results") && !j["results"].empty()) return j["results"][0].value("feedUrl", "");
            } catch (...) {}
            return "";
        }
        
    private:
        static size_t write_cb(void* ptr, size_t size, size_t nmemb, void* data) {
            ((std::string*)data)->append((char*)ptr, size * nmemb);
            return size * nmemb;
        }
    };

    // =========================================================
    // XML属性获取辅助函数
    // =========================================================
    std::string get_xml_prop_any(xmlNodePtr n, std::vector<std::string> ns) {
        for (auto& name : ns) {
            xmlChar* p = xmlGetProp(n, (const xmlChar*)name.c_str());
            if (p) { std::string s = (char*)p; xmlFree(p); return s; }
        }
        return "";
    }

    // =========================================================
    // OPML解析器
    // =========================================================
    class OPMLParser {
    public:
        static TreeNodePtr parse(const std::string& xml) {
            //重置XML错误状态
            reset_xml_error_state();
            
            // V0.05B9n3e5g3h: 添加XML_PARSE_NOWARNING | XML_PARSE_NOERROR选项抑制错误输出到终端
            xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.size(), "opml", NULL, XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
            if (!doc) {
                //记录XML错误
                std::string last_err = get_last_xml_error();
                if (!last_err.empty()) {
                    LOG(fmt::format("[OPML] Parse failed: {}", last_err));
                }
                return nullptr;
            }
            
            auto root = std::make_shared<TreeNode>();
            root->title = "Root";
            root->type = NodeType::FOLDER;
            
            xmlNodePtr rootNode = xmlDocGetRootElement(doc);
            if (rootNode) {
                for (xmlNodePtr n = rootNode->children; n; n = n->next) {
                    if (xmlStrcmp(n->name, (const xmlChar*)"body") == 0) {
                        parse_outline(n->children, root, true);
                    }
                }
            }
            
            xmlFreeDoc(doc);
            return root;
        }
        
        // V0.02: 从OPML文件导入订阅
        static std::vector<TreeNodePtr> import_opml_file(const std::string& filepath) {
            std::vector<TreeNodePtr> feeds;

            if (!fs::exists(filepath)) return feeds;

            std::ifstream f(filepath);
            std::stringstream buffer;
            buffer << f.rdbuf();
            std::string xml = buffer.str();

            if (xml.empty()) return feeds;

            // 解析OPML，提取所有RSS订阅
            extract_feeds(xml, feeds);

            return feeds;
        }

    private:
        static void parse_outline(xmlNodePtr node, TreeNodePtr parent, bool is_top_level = false) {
            (void)is_top_level; // 保留参数供未来使用
            for (; node; node = node->next) {
                if (node->type != XML_ELEMENT_NODE || xmlStrcmp(node->name, (const xmlChar*)"outline")) continue;
                
                auto child = std::make_shared<TreeNode>();
                child->title = get_xml_prop_any(node, {"text", "title"});
                child->parent = parent;  //设置父节点指针
                
                std::string type = get_xml_prop_any(node, {"type"});
                std::string url = get_xml_prop_any(node, {"URL", "url"});
                std::string item = get_xml_prop_any(node, {"item"});
                std::string key = get_xml_prop_any(node, {"key"});
                std::string subtext = get_xml_prop_any(node, {"subtext"});
                std::string duration_str = get_xml_prop_any(node, {"topic_duration"});
                std::string stream_type = get_xml_prop_any(node, {"stream_type"});
                
                child->subtext = subtext;
                if (!duration_str.empty()) {
                    try { child->duration = std::stoi(duration_str); } catch (...) {}
                }
                
                // 检查是否可下载（保留用于未来功能扩展）
                bool is_downloadable = (stream_type == "download");
                (void)is_downloadable; // 保留变量供未来使用
                
                if (item == "topic" && type == "audio") {
                    child->type = NodeType::PODCAST_EPISODE;
                    child->url = url;
                    child->children_loaded = true;
                    parent->children.push_back(child);
                }
                else if (type == "audio" && !url.empty()) {
                    child->type = NodeType::RADIO_STREAM;
                    child->url = url;
                    child->children_loaded = true;
                    parent->children.push_back(child);
                }
                else if (type == "link" || type == "search" || 
                         (!url.empty() && url.find("Browse.ashx") != std::string::npos)) {
                    child->type = NodeType::FOLDER;
                    child->url = url;
                    parent->children.push_back(child);
                }
                else if (node->children != nullptr) {
                    child->type = NodeType::FOLDER;
                    if (!key.empty()) {
                        child->title = child->title.empty() ? key : child->title;
                    }
                    parent->children.push_back(child);
                    parse_outline(node->children, child, false);
                    child->children_loaded = true;
                }
                else if (!url.empty()) {
                    child->type = NodeType::RADIO_STREAM;
                    child->url = url;
                    child->children_loaded = true;
                    parent->children.push_back(child);
                }
            }
        }

        static void extract_feeds(const std::string& xml, std::vector<TreeNodePtr>& feeds) {
            //重置XML错误状态
            reset_xml_error_state();
            
            // V0.05B9n3e5g3h: 添加XML_PARSE_NOWARNING | XML_PARSE_NOERROR选项抑制错误输出到终端
            xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.size(), "opml", NULL, XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
            if (!doc) {
                std::string last_err = get_last_xml_error();
                if (!last_err.empty()) {
                    LOG(fmt::format("[OPML] Extract feeds failed: {}", last_err));
                }
                return;
            }

            xmlNodePtr root = xmlDocGetRootElement(doc);
            if (!root) { xmlFreeDoc(doc); return; }

            std::function<void(xmlNodePtr)> parse_outlines = [&](xmlNodePtr node) {
                for (; node; node = node->next) {
                    if (node->type != XML_ELEMENT_NODE) continue;

                    if (xmlStrcmp(node->name, (const xmlChar*)"outline") == 0) {
                        std::string type = get_xml_prop_any(node, {"type"});
                        std::string url = get_xml_prop_any(node, {"xmlUrl", "URL", "url"});
                        std::string title = get_xml_prop_any(node, {"text", "title"});

                        // RSS订阅类型
                        if ((type == "rss" || !url.empty()) && url.find("http") == 0) {
                            auto feed = std::make_shared<TreeNode>();
                            feed->title = title.empty() ? "Imported Feed" : title;
                            feed->url = url;
                            feed->type = NodeType::PODCAST_FEED;
                            feeds.push_back(feed);
                        }

                        // 递归处理子节点
                        if (node->children) {
                            parse_outlines(node->children);
                        }
                    } else if (node->children) {
                        parse_outlines(node->children);
                    }
                }
            };

            parse_outlines(root);
            xmlFreeDoc(doc);
        }
    };

    // =========================================================
    // RSS解析器
    // =========================================================
    class RSSParser {
    public:
        static TreeNodePtr parse(std::string xml, const std::string& feed_url) {
            //重置XML错误状态
            reset_xml_error_state();
            
            size_t p = xml.find('<');
            if (p != std::string::npos && p > 0) xml = xml.substr(p);
            if (xml.compare(0, 3, "\xEF\xBB\xBF") == 0) xml = xml.substr(3);
            
            // V0.05B9n3e5g3h: 添加XML_PARSE_NOWARNING | XML_PARSE_NOERROR选项抑制错误输出到终端
            // 防止HTML内容导致的parser error输出到终端造成花屏
            xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.size(), "feed", NULL, XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
            if (!doc) {
                //显示更详细的XML错误
                std::string last_err = get_last_xml_error();
                if (!last_err.empty()) {
                    EVENT_LOG(fmt::format("XML parse failed: {}", last_err.substr(0, 50)));
                } else {
                    EVENT_LOG("XML parse failed: invalid document");
                }
                return nullptr;
            }
            
            auto channel = std::make_shared<TreeNode>();
            channel->type = NodeType::PODCAST_FEED;
            channel->children_loaded = true;
            
            xmlNodePtr root = xmlDocGetRootElement(doc);
            if (!root) { xmlFreeDoc(doc); return nullptr; }
            
            bool is_youtube_rss = (feed_url.find("youtube.com/feeds/videos.xml") != std::string::npos);
            
            if (xmlStrcmp(root->name, (const xmlChar*)"rss") == 0 || xmlStrcmp(root->name, (const xmlChar*)"rdf:RDF") == 0) {
                for (xmlNodePtr n = root->children; n; n = n->next) {
                    if (xmlStrcmp(n->name, (const xmlChar*)"channel") == 0) {
                        parse_channel(n, channel);
                    }
                }
            } else if (xmlStrcmp(root->name, (const xmlChar*)"feed") == 0) {
                if (is_youtube_rss) {
                    parse_youtube_atom(doc, channel);
                } else {
                    parse_atom_feed(root, channel);
                }
            }
            
            xmlFreeDoc(doc);
            return channel;
        }
        
    private:
        //增强版channel解析器
        // 支持RSS 2.0 channel + iTunes扩展 + Atom分页
        static void parse_channel(xmlNodePtr ch, TreeNodePtr c) {
            for (xmlNodePtr p = ch->children; p; p = p->next) {
                const char* local_name = (const char*)p->name;
                
                // RSS 2.0 标准字段 - title
                if (strcmp(local_name, "title") == 0) {
                    xmlChar* t = xmlNodeGetContent(p);
                    if (t) { c->title = (char*)t; xmlFree(t); }
                }
                // RSS 2.0 标准字段 - description (播客简介)
                else if (strcmp(local_name, "description") == 0) {
                    // 可以存储播客简介，目前忽略
                }
                // RSS 2.0 标准字段 - item (节目)
                else if (strcmp(local_name, "item") == 0) {
                    auto e = parse_item(p);
                    if (e) { e->parent = c; c->children.push_back(e); }
                }
                // iTunes扩展 - author (作者)
                else if (strcmp(local_name, "itunes:author") == 0) {
                    xmlChar* t = xmlNodeGetContent(p);
                    if (t) {
                        // 可以存储作者信息
                        xmlFree(t);
                    }
                }
                // iTunes扩展 - image (播客封面)
                else if (strcmp(local_name, "itunes:image") == 0) {
                    xmlChar* href = xmlGetProp(p, (const xmlChar*)"href");
                    if (href) {
                        // 可以存储封面URL
                        xmlFree(href);
                    }
                }
                // iTunes扩展 - category (分类)
                else if (strcmp(local_name, "itunes:category") == 0) {
                    xmlChar* txt = xmlGetProp(p, (const xmlChar*)"text");
                    if (txt) {
                        // 可以存储分类信息
                        xmlFree(txt);
                    }
                }
                // Atom扩展 - link (分页/自引用)
                // 命名空间: xmlns:atom="http://www.w3.org/2005/Atom"
                else if (strcmp(local_name, "link") == 0 || 
                         strcmp(local_name, "atom:link") == 0) {
                    xmlChar* rel = xmlGetProp(p, (const xmlChar*)"rel");
                    xmlChar* href = xmlGetProp(p, (const xmlChar*)"href");
                    if (rel && href) {
                        std::string rel_val = (char*)rel;
                        std::string href_val = (char*)href;
                        // atom:link rel="next" - 分页支持
                        if (rel_val == "next" && !href_val.empty()) {
                            // 可以存储下一页URL用于分页加载
                            LOG(fmt::format("[RSS] Found next page: {}", href_val));
                        }
                        xmlFree(rel);
                        xmlFree(href);
                    } else {
                        if (rel) xmlFree(rel);
                        if (href) xmlFree(href);
                    }
                }
            }
        }
        
        //增强版RSS item解析器
        // 支持RSS 2.0、iTunes扩展、Media RSS、Podcast 2.0
        static TreeNodePtr parse_item(xmlNodePtr it) {
            auto ep = std::make_shared<TreeNode>();
            ep->type = NodeType::PODCAST_EPISODE;
            std::string url;  // 音频/视频URL
            std::string media_url;  // Media RSS URL (fallback)
            
            for (xmlNodePtr i = it->children; i; i = i->next) {
                const char* local_name = (const char*)i->name;
                
                // RSS 2.0 标准字段
                if (strcmp(local_name, "title") == 0) {
                    xmlChar* t = xmlNodeGetContent(i);
                    if (t) { ep->title = (char*)t; xmlFree(t); }
                } 
                // RSS 2.0 enclosure - 优先级最高
                else if (strcmp(local_name, "enclosure") == 0) {
                    std::string u = get_xml_prop_any(i, {"url"});
                    if (!u.empty()) url = u;
                }
                // Media RSS support (media:content) - 作为enclosure的fallback
                // 命名空间: xmlns:media="http://search.yahoo.com/mrss/"
                else if (strcmp(local_name, "content") == 0 || 
                         strcmp(local_name, "media:content") == 0) {
                    xmlChar* media_url_attr = xmlGetProp(i, (const xmlChar*)"url");
                    xmlChar* type_attr = xmlGetProp(i, (const xmlChar*)"type");
                    if (media_url_attr) {
                        std::string murl = (char*)media_url_attr;
                        std::string mime = type_attr ? (char*)type_attr : "";
                        // 只接受音频/视频类型的media:content
                        if (mime.find("audio") != std::string::npos || 
                            mime.find("video") != std::string::npos ||
                            murl.find(".mp3") != std::string::npos ||
                            murl.find(".mp4") != std::string::npos ||
                            murl.find(".m4a") != std::string::npos ||
                            murl.find(".ogg") != std::string::npos ||
                            murl.find(".webm") != std::string::npos) {
                            if (media_url.empty()) media_url = murl;
                        }
                        xmlFree(media_url_attr);
                        if (type_attr) xmlFree(type_attr);
                    }
                }
                // Media RSS group - 嵌套的media:content
                else if (strcmp(local_name, "group") == 0 || 
                         strcmp(local_name, "media:group") == 0) {
                    for (xmlNodePtr mc = i->children; mc; mc = mc->next) {
                        const char* mc_name = (const char*)mc->name;
                        if (strcmp(mc_name, "content") == 0 || 
                            strcmp(mc_name, "media:content") == 0) {
                            xmlChar* media_url_attr = xmlGetProp(mc, (const xmlChar*)"url");
                            xmlChar* type_attr = xmlGetProp(mc, (const xmlChar*)"type");
                            if (media_url_attr) {
                                std::string murl = (char*)media_url_attr;
                                std::string mime = type_attr ? (char*)type_attr : "";
                                if ((mime.find("audio") != std::string::npos || 
                                     mime.find("video") != std::string::npos ||
                                     murl.find(".mp3") != std::string::npos ||
                                     murl.find(".m4a") != std::string::npos) && 
                                    media_url.empty()) {
                                    media_url = murl;
                                }
                                xmlFree(media_url_attr);
                                if (type_attr) xmlFree(type_attr);
                            }
                        }
                    }
                }
                // link字段 - 最后的fallback
                else if (strcmp(local_name, "link") == 0 && url.empty() && media_url.empty()) {
                    xmlChar* t = xmlNodeGetContent(i);
                    if (t) { 
                        std::string l = (char*)t; 
                        xmlFree(t); 
                        if (l.find(".mp3") != std::string::npos || 
                            l.find(".mp4") != std::string::npos ||
                            l.find(".m4a") != std::string::npos) url = l; 
                    }
                }
                // pubDate / dc:date - 发布日期
                else if (strcmp(local_name, "pubDate") == 0 || 
                         strcmp(local_name, "dc:date") == 0) {
                    xmlChar* t = xmlNodeGetContent(i);
                    if (t) { ep->subtext = (char*)t; xmlFree(t); }
                }
                // iTunes扩展 - duration
                else if (strcmp(local_name, "duration") == 0 ||
                         strcmp(local_name, "itunes:duration") == 0) {
                    xmlChar* t = xmlNodeGetContent(i);
                    if (t) {
                        std::string dur = (char*)t;
                        xmlFree(t);
                        int d = 0;
                        size_t colon1 = dur.find(':');
                        if (colon1 != std::string::npos) {
                            size_t colon2 = dur.find(':', colon1 + 1);
                            if (colon2 != std::string::npos) {
                                d = std::stoi(dur.substr(0, colon1)) * 3600 +
                                    std::stoi(dur.substr(colon1 + 1, colon2 - colon1 - 1)) * 60 +
                                    std::stoi(dur.substr(colon2 + 1));
                            } else {
                                d = std::stoi(dur.substr(0, colon1)) * 60 +
                                    std::stoi(dur.substr(colon1 + 1));
                            }
                        } else {
                            try { d = std::stoi(dur); } catch (...) {}
                        }
                        ep->duration = d;
                    }
                }
                // iTunes扩展 - author (存储在额外字段，暂不使用)
                else if (strcmp(local_name, "author") == 0 ||
                         strcmp(local_name, "itunes:author") == 0) {
                    // 可以存储作者信息，目前忽略
                }
                // iTunes扩展 - image (封面图)
                else if (strcmp(local_name, "itunes:image") == 0) {
                    xmlChar* href = xmlGetProp(i, (const xmlChar*)"href");
                    if (href) {
                        // 可以存储封面URL，目前忽略
                        xmlFree(href);
                    }
                }
                // Podcast 2.0扩展 - chapters (章节文件)
                else if (strcmp(local_name, "chapters") == 0 ||
                         strcmp(local_name, "podcast:chapters") == 0) {
                    xmlChar* ch_url = xmlGetProp(i, (const xmlChar*)"url");
                    if (ch_url) {
                        // 可以存储章节URL，目前忽略
                        xmlFree(ch_url);
                    }
                }
                // Podcast 2.0扩展 - transcript (字幕/转录)
                else if (strcmp(local_name, "transcript") == 0 ||
                         strcmp(local_name, "podcast:transcript") == 0) {
                    xmlChar* tr_url = xmlGetProp(i, (const xmlChar*)"url");
                    if (tr_url) {
                        // 可以存储字幕URL，目前忽略
                        xmlFree(tr_url);
                    }
                }
            }
            
            // URL解析优先级策略: enclosure > media:content > link
            std::string final_url = url.empty() ? media_url : url;
            
            if (!final_url.empty()) {
                ep->url = final_url;
                ep->children_loaded = true;
                // 检测视频URL
                URLType url_type = URLClassifier::classify(final_url);
                if (url_type == URLType::VIDEO_FILE) {
                    ep->is_youtube = false; // 不是YouTube但是视频
                }
                return ep;
            }
            return nullptr;
        }
        
        static void parse_atom_feed(xmlNodePtr feed, TreeNodePtr c) {
            for (xmlNodePtr n = feed->children; n; n = n->next) {
                if (xmlStrcmp(n->name, (const xmlChar*)"title") == 0) {
                    xmlChar* t = xmlNodeGetContent(n);
                    if (t) { c->title = (char*)t; xmlFree(t); }
                } else if (xmlStrcmp(n->name, (const xmlChar*)"entry") == 0) {
                    auto e = parse_atom_entry(n);
                    if (e) { e->parent = c; c->children.push_back(e); }
                }
            }
        }
        
        static TreeNodePtr parse_atom_entry(xmlNodePtr entry) {
            auto ep = std::make_shared<TreeNode>();
            ep->type = NodeType::PODCAST_EPISODE;
            
            for (xmlNodePtr child = entry->children; child; child = child->next) {
                if (child->type != XML_ELEMENT_NODE) continue;
                
                const char* name = (const char*)child->name;
                if (strcmp(name, "title") == 0) {
                    xmlChar* t = xmlNodeGetContent(child);
                    if (t) { ep->title = (char*)t; xmlFree(t); }
                } else if (strcmp(name, "link") == 0) {
                    xmlChar* href = xmlGetProp(child, (const xmlChar*)"href");
                    xmlChar* type = xmlGetProp(child, (const xmlChar*)"type");
                    if (href) {
                        std::string link = (char*)href;
                        std::string mime = type ? (char*)type : "";
                        if (mime.find("audio") != std::string::npos || 
                            mime.find("video") != std::string::npos ||
                            link.find(".mp3") != std::string::npos ||
                            link.find(".mp4") != std::string::npos ||
                            link.find(".m4a") != std::string::npos) {
                            ep->url = link;
                        }
                        xmlFree(href);
                        if (type) xmlFree(type);
                    }
                } else if (strcmp(name, "published") == 0 || strcmp(name, "updated") == 0) {
                    xmlChar* t = xmlNodeGetContent(child);
                    if (t) { ep->subtext = (char*)t; xmlFree(t); }
                }
            }
            
            if (!ep->url.empty()) return ep;
            return nullptr;
        }
        
        static void parse_youtube_atom(xmlDocPtr doc, TreeNodePtr c) {
            xmlXPathContextPtr xpath_ctx = xmlXPathNewContext(doc);
            if (!xpath_ctx) return;
            
            xmlXPathRegisterNs(xpath_ctx, (const xmlChar*)"atom", (const xmlChar*)"http://www.w3.org/2005/Atom");
            xmlXPathRegisterNs(xpath_ctx, (const xmlChar*)"yt", (const xmlChar*)"http://www.youtube.com/xml/schemas/2015");
            xmlXPathRegisterNs(xpath_ctx, (const xmlChar*)"media", (const xmlChar*)"http://search.yahoo.com/mrss/");
            
            xmlXPathObjectPtr title_result = xmlXPathNodeEval(
                xmlDocGetRootElement(doc),
                (const xmlChar*)".//*[local-name()='title'][1]",
                xpath_ctx);
            
            if (title_result && title_result->nodesetval && title_result->nodesetval->nodeNr > 0) {
                xmlChar* t = xmlNodeGetContent(title_result->nodesetval->nodeTab[0]);
                if (t) { c->title = (char*)t; xmlFree(t); }
            }
            if (title_result) xmlXPathFreeObject(title_result);
            
            xmlXPathObjectPtr entries = xmlXPathNodeEval(
                xmlDocGetRootElement(doc),
                (const xmlChar*)".//*[local-name()='entry']",
                xpath_ctx);
            
            if (entries && entries->nodesetval) {
                for (int i = 0; i < entries->nodesetval->nodeNr; ++i) {
                    auto ep = parse_youtube_entry(entries->nodesetval->nodeTab[i]);
                    if (ep) { ep->parent = c; c->children.push_back(ep); }
                }
            }
            
            if (entries) xmlXPathFreeObject(entries);
            xmlXPathFreeContext(xpath_ctx);
        }
        
        static TreeNodePtr parse_youtube_entry(xmlNodePtr entry) {
            auto ep = std::make_shared<TreeNode>();
            ep->type = NodeType::PODCAST_EPISODE;
            ep->is_youtube = true;
            std::string video_id, title, link_url;
            
            for (xmlNodePtr child = entry->children; child; child = child->next) {
                if (child->type != XML_ELEMENT_NODE) continue;
                const char* local_name = (const char*)child->name;
                
                if (strcmp(local_name, "title") == 0 && title.empty()) {
                    xmlChar* t = xmlNodeGetContent(child);
                    if (t) { title = (char*)t; xmlFree(t); }
                } else if (strcmp(local_name, "videoId") == 0) {
                    xmlChar* vid = xmlNodeGetContent(child);
                    if (vid) { std::string vid_str = (char*)vid; if (vid_str.length() == 11) video_id = vid_str; xmlFree(vid); }
                } else if (strcmp(local_name, "link") == 0) {
                    xmlChar* href = xmlGetProp(child, (const xmlChar*)"href");
                    xmlChar* rel = xmlGetProp(child, (const xmlChar*)"rel");
                    if (href && (!rel || strcmp((char*)rel, "alternate") == 0)) link_url = (char*)href;
                    if (href) xmlFree(href);
                    if (rel) xmlFree(rel);
                }
            }
            
            if (!video_id.empty()) ep->url = fmt::format("https://www.youtube.com/watch?v={}", video_id);
            else if (!link_url.empty()) ep->url = link_url;
            ep->title = title;
            
            if (!ep->title.empty() && !ep->url.empty()) return ep;
            return nullptr;
        }
    };

    // =========================================================
    // YouTube频道解析器 - V0.05B9n3e5g2RF8A20: 简化版（参考A版本）
    // =========================================================
    // 设计原则：
    // 1. 保持简单，使用最基本的yt-dlp命令
    // 2. 使用2>/dev/null隐藏错误输出，避免解析干扰
    // 3. 不添加额外参数，让yt-dlp自动处理
    // =========================================================
    class YouTubeChannelParser {
    public:
        static TreeNodePtr parse(const std::string& channel_url) {
            auto channel_node = std::make_shared<TreeNode>();
            channel_node->type = NodeType::PODCAST_FEED;
            channel_node->children_loaded = false;
            channel_node->is_youtube = true;
            channel_node->title = URLClassifier::extract_channel_name(channel_url);
            channel_node->channel_name = channel_node->title;
            
            EVENT_LOG(fmt::format("YouTube Channel: {}", channel_url));
            LOG(fmt::format("[YouTube] Parsing: {}", channel_url));
            
            //使用最简单的命令（参考A版本成功经验）
            // --flat-playlist: 快速获取列表，不下载
            // --dump-json: 输出JSON格式
            // --no-warnings: 禁用警告输出
            // 2>&1: 将stderr重定向到stdout，统一捕获所有输出
            // 这样可以避免任何终端输出导致花屏
            std::string cmd = fmt::format("yt-dlp --flat-playlist --dump-json --no-warnings \"{}\" 2>&1", channel_url);
            FILE* fp = popen(cmd.c_str(), "r");
            if (!fp) {
                channel_node->parse_failed = true;
                channel_node->error_msg = "Failed to execute yt-dlp";
                EVENT_LOG("YouTube: Failed to execute yt-dlp");
                return channel_node;
            }
            
            char buf[8192];
            int count = 0;
            std::vector<YouTubeVideoInfo> videos;
            
            while (fgets(buf, sizeof(buf), fp)) {
                try {
                    std::string line = buf;
                    // 移除换行符
                    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                        line.pop_back();
                    }
                    // 跳过空行和非JSON行
                    if (line.empty() || line[0] != '{') continue;
                    
                    auto j = json::parse(line);
                    std::string id = j.value("id", "");
                    std::string title = j.value("title", "Untitled");
                    
                    if (!id.empty()) {
                        auto ep = std::make_shared<TreeNode>();
                        ep->type = NodeType::PODCAST_EPISODE;
                        ep->title = title;
                        ep->url = fmt::format("https://www.youtube.com/watch?v={}", id);
                        ep->is_youtube = true;
                        ep->children_loaded = true;
                        ep->parent = channel_node;
                        channel_node->children.push_back(ep);
                        videos.push_back({id, title, ep->url});
                        count++;
                    }
                } catch (...) {
                    // 解析错误时跳过该行
                }
            }
            
            pclose(fp);
            
            if (count > 0) {
                channel_node->children_loaded = true;
                YouTubeCache::instance().update(channel_url, channel_node->title, videos);
                EVENT_LOG(fmt::format("Found {} videos", count));
                LOG(fmt::format("[YouTube] Parsed {} videos", count));
            } else {
                channel_node->parse_failed = true;
                channel_node->error_msg = "No videos found";
                EVENT_LOG("YouTube: No videos found");
            }
            
            return channel_node;
        }
    };

    // =========================================================
    // 持久化
    // =========================================================
    class Persistence {
    public:
        //保存RADIO树到数据库（不再使用cache.json）
        static void save_cache(const TreeNodePtr& radio, const TreeNodePtr& podcast) {
            (void)podcast; // PODCAST树已通过save_data()保存到nodes表
            // 保存RADIO树到数据库
            if (radio) {
                DatabaseManager::instance().save_radio_cache(radio);
            }
            // PODCAST树已通过save_data()保存到nodes表
        }
        
        //从数据库加载RADIO树（不再使用cache.json）
        static void load_cache(TreeNodePtr& radio, TreeNodePtr& podcast) {
            (void)podcast; // PODCAST树已通过load_data()从nodes表加载
            // 从数据库加载RADIO树
            radio = DatabaseManager::instance().load_radio_cache();
            // PODCAST树已通过load_data()从nodes表加载
        }
        
        //保存订阅和收藏到数据库
        // 关键修复：先清空nodes表中的订阅数据，再保存当前数据
        // 这样删除订阅后保存才能真正生效
        static void save_data(const std::vector<TreeNodePtr>& podcasts, const std::vector<TreeNodePtr>& favs) {
            //先清空nodes表中的订阅数据
            DatabaseManager::instance().clear_nodes();
            
            // 保存订阅到nodes表
            for (const auto& p : podcasts) {
                if (p && !p->url.empty()) {
                    json data_json;
                    data_json["expanded"] = p->expanded;
                    data_json["children_loaded"] = p->children_loaded;
                    data_json["is_youtube"] = p->is_youtube;
                    data_json["channel_name"] = p->channel_name;
                    data_json["is_cached"] = p->is_cached;
                    data_json["is_downloaded"] = p->is_downloaded;
                    data_json["local_file"] = p->local_file;
                    
                    // 保存子节点信息
                    if (!p->children.empty()) {
                        data_json["children"] = json::array();
                        for (const auto& c : p->children) {
                            json child_json;
                            child_json["title"] = c->title;
                            child_json["url"] = c->url;
                            child_json["type"] = (int)c->type;
                            child_json["duration"] = c->duration;
                            data_json["children"].push_back(child_json);
                        }
                    }
                    
                    DatabaseManager::instance().save_node(
                        p->url, p->title, (int)p->type, data_json.dump()
                    );
                }
            }
            
            //先清空favourites表
            DatabaseManager::instance().clear_favourites();
            
            // 保存收藏到favourites表
            for (const auto& f : favs) {
                if (f) {
                    json data_json;
                    data_json["children_loaded"] = f->children_loaded;
                    if (!f->children.empty()) {
                        data_json["children"] = json::array();
                        for (const auto& c : f->children) {
                            json child_json;
                            child_json["title"] = c->title;
                            child_json["url"] = c->url;
                            child_json["type"] = (int)c->type;
                            data_json["children"].push_back(child_json);
                        }
                    }
                    
                    DatabaseManager::instance().save_favourite(
                        f->title, 
                        f->url, 
                        (int)f->type, 
                        f->is_youtube, 
                        f->channel_name,
                        "",  // source_type
                        data_json.dump()
                    );
                }
            }
        }
        
        //从数据库加载订阅和收藏
        // 关键修复：从nodes表加载订阅数据，而不仅仅依赖migrate_from_json
        static void load_data(std::vector<TreeNodePtr>& podcasts, std::vector<TreeNodePtr>& favs) {
            //首先尝试从数据库nodes表加载订阅
            auto db_podcasts = DatabaseManager::instance().load_nodes();
            if (!db_podcasts.empty()) {
                // 从数据库加载成功，不需要迁移
                for (const auto& [title, url, type, data_json] : db_podcasts) {
                    auto node = std::make_shared<TreeNode>();
                    node->title = title;
                    node->url = url;
                    node->type = (NodeType)type;
                    
                    // 解析data_json
                    if (!data_json.empty()) {
                        try {
                            json j = json::parse(data_json);
                            node->expanded = j.value("expanded", false);
                            node->children_loaded = j.value("children_loaded", false);
                            node->is_youtube = j.value("is_youtube", false);
                            node->channel_name = j.value("channel_name", "");
                            node->is_cached = j.value("is_cached", false);
                            node->is_downloaded = j.value("is_downloaded", false);
                            node->local_file = j.value("local_file", "");
                            
                            // 加载子节点
                            if (j.contains("children") && j["children"].is_array()) {
                                for (const auto& c : j["children"]) {
                                    auto child = std::make_shared<TreeNode>();
                                    child->title = c.value("title", "");
                                    child->url = c.value("url", "");
                                    child->type = (NodeType)c.value("type", 0);
                                    child->duration = c.value("duration", 0);
                                    child->children_loaded = true;
                                    child->parent = node;  //设置父节点指针
                                    node->children.push_back(child);
                                }
                            }
                        } catch (...) {}
                    }
                    
                    podcasts.push_back(node);
                }
            } else {
                // 数据库为空，尝试从旧的data.json迁移
                migrate_from_json(podcasts, favs);
            }
            
            // 从数据库加载收藏
            auto db_favs = DatabaseManager::instance().load_favourites();
            for (const auto& [title, url, type, is_youtube, channel_name, source_type, data_json] : db_favs) {
                auto node = std::make_shared<TreeNode>();
                node->title = title;
                node->url = url;
                node->type = (NodeType)type;
                node->is_youtube = is_youtube;
                node->channel_name = channel_name;
                node->children_loaded = true;
                
                //修复LINK加载 - 从data_json解析LINK信息
                // 保存收藏时data_json包含: {"is_link":true, "source_mode":"RADIO", "link_target_url":"..."}
                if (!data_json.empty()) {
                    try {
                        json j = json::parse(data_json);
                        
                        // 关键修复：从data_json恢复is_link字段
                        if (j.value("is_link", false)) {
                            node->is_link = true;
                            node->source_mode = j.value("source_mode", source_type);
                            node->link_target_url = j.value("link_target_url", url);
                            node->children_loaded = false;  // LINK节点延迟加载
                            
                            // 特殊处理：online_root
                            if (url == "online_root" || node->link_target_url == "online_root") {
                                node->linked_node = OnlineState::instance().online_root;
                            }
                        } else {
                            // 非LINK节点：解析子节点
                            node->children_loaded = j.value("children_loaded", true);
                            if (j.contains("children") && j["children"].is_array()) {
                                for (const auto& c : j["children"]) {
                                    auto child = std::make_shared<TreeNode>();
                                    child->title = c.value("title", "");
                                    child->url = c.value("url", "");
                                    child->type = (NodeType)c.value("type", 0);
                                    child->children_loaded = true;
                                    child->parent = node;
                                    node->children.push_back(child);
                                }
                            }
                        }
                    } catch (...) {
                        // 解析失败，使用旧的source_type判断逻辑作为fallback
                        if (source_type == "online_root_link" || url == "online_root") {
                            node->is_link = true;
                            node->linked_node = OnlineState::instance().online_root;
                            node->children_loaded = false;
                        } else if (source_type == "search_record_link" && url.find("search:") == 0) {
                            node->is_link = true;
                            node->children_loaded = false;
                        }
                    }
                }
                
                favs.push_back(node);
            }
        }
        
        //从旧data.json迁移到数据库
        //移除了对Utils::get_data_file()的依赖，直接构建路径
        static void migrate_from_json(std::vector<TreeNodePtr>& podcasts, std::vector<TreeNodePtr>& favs) {
            const char* home = std::getenv("HOME");
            if (!home) return;
            
            std::string path = std::string(home) + DATA_DIR + "/data.json";
            if (!fs::exists(path)) return;
            
            try {
                std::ifstream f(path);
                json j; f >> j;
                
                // 迁移订阅
                if (j.contains("podcasts")) {
                    for (const auto& p : j["podcasts"]) {
                        podcasts.push_back(load_node(p));
                    }
                }
                
                // 迁移收藏
                if (j.contains("favourites")) {
                    for (const auto& fav : j["favourites"]) {
                        auto node = load_node(fav);
                        favs.push_back(node);
                        
                        // 同时保存到数据库
                        json data_json;
                        data_json["children_loaded"] = node->children_loaded;
                        if (!node->children.empty()) {
                            data_json["children"] = json::array();
                            for (const auto& c : node->children) {
                                json child_json;
                                child_json["title"] = c->title;
                                child_json["url"] = c->url;
                                child_json["type"] = (int)c->type;
                                data_json["children"].push_back(child_json);
                            }
                        }
                        
                        DatabaseManager::instance().save_favourite(
                            node->title, node->url, (int)node->type,
                            node->is_youtube, node->channel_name, "", data_json.dump()
                        );
                    }
                }
                
                // 迁移完成后重命名旧文件
                std::string backup_path = path + ".migrated";
                fs::rename(path, backup_path);
                LOG(fmt::format("[Migration] data.json migrated to database, backup saved to {}", backup_path));
                
            } catch (...) {}
        }
        
        //修复导出函数，使用std::cout替代EVENT_LOG（兼容命令行模式）
        static void export_opml(const std::string& filename, const std::vector<TreeNodePtr>& podcasts) {
            std::ofstream f(filename);
            if (!f.is_open()) {
                std::cout << "Error: Cannot open file for writing: " << filename << std::endl;
                return;
            }
            f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            f << "<opml version=\"1.0\">\n";
            f << "<head><title>PODRADIO Export</title></head>\n";
            f << "<body>\n";
            int count = 0;
            for (const auto& p : podcasts) {
                if (p && !p->url.empty()) {  //添加空指针检查
                    f << fmt::format("  <outline text=\"{}\" title=\"{}\" type=\"rss\" xmlUrl=\"{}\"/>\n",
                                    p->title, p->title, p->url);
                    count++;
                }
            }
            f << "</body>\n</opml>\n";
            f.close();
            std::cout << "Exported " << count << " podcasts to " << filename << std::endl;
        }
        
    private:
        static json save_tree(const TreeNodePtr& node) {
            json j = {{"title", node->title}, {"url", node->url}, {"type", (int)node->type},
                      {"expanded", node->expanded}, {"children_loaded", node->children_loaded},
                      {"is_youtube", node->is_youtube}, {"channel_name", node->channel_name},
                      {"is_cached", node->is_cached}, {"is_downloaded", node->is_downloaded},
                      {"local_file", node->local_file}};
            j["children"] = json::array();
            for (const auto& c : node->children) j["children"].push_back(save_tree(c));
            return j;
        }
        
        static void load_tree(TreeNodePtr& node, const json& j) {
            node->title = j.value("title", "");
            node->url = j.value("url", "");
            node->type = (NodeType)j.value("type", 0);
            node->expanded = j.value("expanded", false);
            node->children_loaded = j.value("children_loaded", false);
            node->is_youtube = j.value("is_youtube", false);
            node->channel_name = j.value("channel_name", "");
            node->is_cached = j.value("is_cached", false);
            node->is_downloaded = j.value("is_downloaded", false);
            node->local_file = j.value("local_file", "");
            if (j.contains("children")) for (const auto& c : j["children"]) {
                auto child = std::make_shared<TreeNode>();
                load_tree(child, c);
                child->parent = node;  //设置父节点指针
                node->children.push_back(child);
            }
        }
        
        static json save_node(const TreeNodePtr& node) {
            return {{"title", node->title}, {"url", node->url}, {"type", (int)node->type}, 
                    {"is_youtube", node->is_youtube}, {"is_cached", node->is_cached},
                    {"is_downloaded", node->is_downloaded}, {"local_file", node->local_file}};
        }
        
        static TreeNodePtr load_node(const json& j) {
            auto node = std::make_shared<TreeNode>();
            node->title = j.value("title", "");
            node->url = j.value("url", "");
            node->type = (NodeType)j.value("type", 0);
            node->is_youtube = j.value("is_youtube", false);
            node->is_cached = j.value("is_cached", false);
            node->is_downloaded = j.value("is_downloaded", false);
            node->local_file = j.value("local_file", "");
            return node;
        }
    };

    // =========================================================
    // 状态栏颜色渲染器
    // =========================================================
    class StatusBarColorRenderer {
    public:
        static int get_color(const StatusBarColorConfig& config, int offset) {
            static int hue = 0;
            static auto last_update = std::chrono::steady_clock::now();
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
            
            if (elapsed >= config.update_interval_ms) {
                hue = (hue + config.rainbow_speed) % 360;
                last_update = now;
            }
            
            float brightness = calculate_brightness(config);
            
            switch (config.mode) {
                case StatusBarColorMode::RAINBOW:
                    return get_rainbow_color(hue + offset, brightness);
                case StatusBarColorMode::RANDOM:
                    return get_random_color(brightness);
                case StatusBarColorMode::TIME_BRIGHTNESS:
                    return get_time_adjusted_color(brightness);
                case StatusBarColorMode::FIXED:
                    return get_fixed_color(config.fixed_color, brightness);
                default:
                    return get_rainbow_color(hue + offset, brightness);
            }
        }
        
    private:
        static float calculate_brightness(const StatusBarColorConfig& config) {
            float brightness = config.brightness_max;
            
            if (config.time_adjust) {
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                std::tm* tm = std::localtime(&t);
                int hour = tm->tm_hour;
                
                if (hour >= 22 || hour < 6) {
                    brightness = config.brightness_min;
                } else if (hour >= 18 || hour < 8) {
                    brightness = (config.brightness_min + config.brightness_max) / 2;
                }
            }
            
            return brightness;
        }
        
        static int get_rainbow_color(int hue, float brightness) {
            float h = hue / 60.0f;
            int i = static_cast<int>(h) % 6;
            float f = h - static_cast<int>(h);
            float v = brightness;
            float s = 1.0f;
            
            float p = v * (1 - s);
            float q = v * (1 - f * s);
            float t = v * (1 - (1 - f) * s);
            
            float r, g, b;
            switch (i) {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                default: r = v; g = p; b = q; break;
            }
            
            return 16 + static_cast<int>(r * 5) * 36 + static_cast<int>(g * 5) * 6 + static_cast<int>(b * 5);
        }
        
        static int get_random_color(float brightness) {
            (void)brightness; // 保留参数供未来使用
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> dis(16, 231);
            return dis(gen);
        }
        
        static int get_time_adjusted_color(float brightness) {
            auto now = std::chrono::system_clock::now();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            int hue = (seconds / 10) % 360;
            return get_rainbow_color(hue, brightness);
        }
        
        static int get_fixed_color(const std::string& color_name, float brightness) {
            (void)brightness; // 保留参数供未来使用
            static std::map<std::string, int> color_map = {
                {"black", COLOR_BLACK}, {"red", COLOR_RED}, {"green", COLOR_GREEN},
                {"yellow", COLOR_YELLOW}, {"blue", COLOR_BLUE}, {"magenta", COLOR_MAGENTA},
                {"cyan", COLOR_CYAN}, {"white", COLOR_WHITE},
            };
            
            auto it = color_map.find(color_name);
            if (it != color_map.end()) return it->second;
            return COLOR_CYAN;
        }
    };

    // =========================================================
    // MPV Controller - V0.05B9n3e5g2RF8A9: 新增EndFileCallback
    // =========================================================
    class MPVController {
    public:
        //播放结束回调类型
        // reason: 0=EOF(正常结束), 1=stop, 2=quit, 3=error, 4=redirect, 5=interrupted
        using EndFileCallback = std::function<void(int reason)>;
        
        ~MPVController() {
            running_ = false;
            if (mpv_thread_.joinable()) mpv_thread_.join();
            if (ctx_) mpv_terminate_destroy(ctx_);
        }
        
        bool initialize() {
            //只切换LC_NUMERIC，保护LC_CTYPE宽字符环境
            // mpv_create()会检测locale并打印警告，但只需要LC_NUMERIC为"C"
            setlocale(LC_NUMERIC, "C");
            ctx_ = mpv_create();
            setlocale(LC_NUMERIC, "");  // 恢复

            if (!ctx_) {
                LOG("[MPV] Failed to create context");
                return false;
            }
            
            ytdl_path_ = Utils::execute_command("which yt-dlp");
            while (!ytdl_path_.empty() && (ytdl_path_.back() == '\n' || ytdl_path_.back() == '\r')) ytdl_path_.pop_back();
            ytdl_available_ = !ytdl_path_.empty();
            LOG(fmt::format("[MPV] yt-dlp: {}", ytdl_available_ ? ytdl_path_ : "not found"));
            
            // 默认不打开窗口（适合音频播放）
            mpv_set_option_string(ctx_, "force-window", "no");
            mpv_set_option_string(ctx_, "vo", "null");
            mpv_set_option_string(ctx_, "keep-open", "yes");
            mpv_set_option_string(ctx_, "idle", "once");
            
            //禁用mpv的终端输出，避免花屏
            // msg-level: 设置日志级别为error，只显示严重错误
            // quiet: 禁用大多数终端输出
            mpv_set_option_string(ctx_, "msg-level", "all=error");
            mpv_set_option_string(ctx_, "quiet", "yes");
            
            mpv_set_option_string(ctx_, "ytdl", "yes");
            if (ytdl_available_) {
                mpv_set_option_string(ctx_, "ytdl-path", ytdl_path_.c_str());
            }
            
            mpv_set_option_string(ctx_, "ytdl-format", "bestvideo+bestaudio/best");
            LOG("[MPV] ytdl-format: bestvideo+bestaudio/best");
            
            if (mpv_initialize(ctx_) < 0) return false;
            
            mpv_observe_property(ctx_, 0, "pause", MPV_FORMAT_FLAG);
            mpv_observe_property(ctx_, 0, "volume", MPV_FORMAT_INT64);
            mpv_observe_property(ctx_, 0, "media-title", MPV_FORMAT_STRING);
            mpv_observe_property(ctx_, 0, "path", MPV_FORMAT_STRING);
            mpv_observe_property(ctx_, 0, "core-idle", MPV_FORMAT_FLAG);
            mpv_observe_property(ctx_, 0, "audio-codec", MPV_FORMAT_STRING);
            mpv_observe_property(ctx_, 0, "video-codec", MPV_FORMAT_STRING);
            
            running_ = true;
            mpv_thread_ = std::thread(&MPVController::event_loop, this);
            return true;
        }
        
        void play_audio(const std::string& url) {
            if (url.empty()) {
                LOG("[MPV] Error: Empty URL");
                return;
            }
            // V0.05: play_audio不显示EVENT LOG，由调用方显示OPERATOR COMMAND
            LOG(fmt::format("[MPV] Playing audio: {}", url));

            // V2.39-FF: 确保MPV上下文有效
            if (!ctx_) {
                LOG("[MPV] Error: ctx_ is null");
                return;
            }

            //单文件模式设置keep-open=yes
            // 便于暂停和进度控制
            mpv_set_option_string(ctx_, "keep-open", "yes");
            LOG("[MPV] Single file mode: keep-open=yes");

            mpv_set_option_string(ctx_, "force-window", "no");
            mpv_set_option_string(ctx_, "vo", "null");

            const char* cmd[] = {"loadfile", url.c_str(), "replace", nullptr};
            int result = mpv_command(ctx_, cmd);
            LOG(fmt::format("[MPV] loadfile result: {}", result));

            //确保播放开始（不暂停）
            int pause_val = 0;
            mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
            LOG("[MPV] Ensured playing (pause=no)");
        }
        
        void play_video(const std::string& url) {
            if (url.empty()) {
                LOG("[MPV] Error: Empty URL");
                return;
            }
            // V0.05: play_video不显示EVENT LOG，由调用方显示OPERATOR COMMAND
            LOG(fmt::format("[MPV] Playing video: {}", url));

            // V2.39-FF: 确保MPV上下文有效
            if (!ctx_) {
                LOG("[MPV] Error: ctx_ is null");
                return;
            }

            //单文件模式设置keep-open=yes
            // 便于暂停和进度控制
            mpv_set_option_string(ctx_, "keep-open", "yes");
            LOG("[MPV] Single file mode: keep-open=yes");

            if (Utils::has_gui()) {
                mpv_set_option_string(ctx_, "vo", "gpu");
                mpv_set_option_string(ctx_, "force-window", "yes");
                LOG("[MPV] Video mode: opening window");
            } else {
                mpv_set_option_string(ctx_, "vo", "null");
                mpv_set_option_string(ctx_, "force-window", "no");
                LOG("[MPV] TTY mode: no video window");
            }

            const char* cmd[] = {"loadfile", url.c_str(), "replace", nullptr};
            int result = mpv_command(ctx_, cmd);
            LOG(fmt::format("[MPV] loadfile result: {}", result));

            //确保播放开始（不暂停）
            int pause_val = 0;
            mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
            LOG("[MPV] Ensured playing (pause=no)");
        }
        
        void play(const std::string& url, bool force_video = false) {
            URLType type = URLClassifier::classify(url);

            // V2.39-FF: 改进视频检测逻辑
            // 1. 强制视频模式
            // 2. 明确的视频文件URL (.mp4, .webm, etc)
            // 3. YouTube视频URL (watch?v=)
            bool should_play_video = force_video ||
                                     type == URLType::VIDEO_FILE ||
                                     type == URLType::YOUTUBE_VIDEO;

            // V2.39-FF: YouTube频道/RSS在GUI环境也打开窗口
            // 因为YouTube内容通常是视频
            if (Utils::has_gui() && URLClassifier::is_youtube(type)) {
                should_play_video = true;
            }

            if (should_play_video && Utils::has_gui()) {
                play_video(url);
            } else {
                // V2.39-FF: 音频默认后台播放，不打开窗口
                play_audio(url);
            }
        }

        //自动播放专用方法 - 用于播放列表自动播放下一项
        // 关键区别：设置keep-open=no，确保播放结束后能继续触发END_FILE事件
        void play_for_autoplay(const std::string& url, bool is_video = false) {
            if (url.empty()) {
                LOG("[MPV] play_for_autoplay: Empty URL");
                return;
            }

            LOG(fmt::format("[MPV] Autoplay: {}", url));

            //关键设置：keep-open=no允许播放结束后触发END_FILE事件
            // 这使得自动播放链能够继续工作
            mpv_set_option_string(ctx_, "keep-open", "no");
            LOG("[MPV] Autoplay mode: keep-open=no for continuous playback");

            if (is_video && Utils::has_gui()) {
                mpv_set_option_string(ctx_, "vo", "gpu");
                mpv_set_option_string(ctx_, "force-window", "yes");
            } else {
                mpv_set_option_string(ctx_, "force-window", "no");
                mpv_set_option_string(ctx_, "vo", "null");
            }

            const char* cmd[] = {"loadfile", url.c_str(), "replace", nullptr};
            int result = mpv_command(ctx_, cmd);
            LOG(fmt::format("[MPV] Autoplay loadfile result: {}", result));

            //确保播放开始
            int pause_val = 0;
            mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
            LOG("[MPV] Autoplay: Ensured playing (pause=no)");
        }
        
        void play_hybrid(const std::string& local_file, const std::string& online_url) {
            // V0.05: 不显示EVENT LOG，只记录日志
            LOG(fmt::format("[MPV] Hybrid: {} + {}", local_file, online_url));
            
            std::string tmp_playlist = fmt::format("/tmp/podradio_hybrid_{}.m3u", getpid());
            std::ofstream f(tmp_playlist);
            if (f.is_open()) {
                f << local_file << "\n";
                f << online_url << "\n";
                f.close();
                
                if (Utils::has_gui()) {
                    mpv_set_option_string(ctx_, "vo", "gpu");
                    mpv_set_option_string(ctx_, "force-window", "yes");
                }
                
                const char* cmd[] = {"loadlist", tmp_playlist.c_str(), "replace", nullptr};
                mpv_command(ctx_, cmd);
            } else {
                play_video(online_url);
            }
        }
        
        //无缝切换播放 - 优先播放本地缓存，无缝切换到在线流
        void play_seamless(const std::string& local_file, const std::string& online_url, bool is_video = false) {
            LOG(fmt::format("[MPV] Seamless: local={}, online={}", local_file, online_url));
            EVENT_LOG(fmt::format("Seamless Play: {} -> online", fs::path(local_file).filename().string()));
            
            //设置gapless模式实现无缝切换
            mpv_set_option_string(ctx_, "gapless-audio", "yes");
            EVENT_LOG("PRE-FETCHING: gapless-audio enabled");
            
            //预加载播放列表下一项
            mpv_set_option_string(ctx_, "prefetch-playlist", "yes");
            EVENT_LOG("PRE-FETCHING: prefetch-playlist enabled");
            
            if (is_video && Utils::has_gui()) {
                mpv_set_option_string(ctx_, "vo", "gpu");
                mpv_set_option_string(ctx_, "force-window", "yes");
            } else {
                mpv_set_option_string(ctx_, "force-window", "no");
                mpv_set_option_string(ctx_, "vo", "null");
            }
            
            // 创建播放列表：本地缓存 -> 在线流
            std::string tmp_playlist = fmt::format("/tmp/podradio_seamless_{}.m3u", getpid());
            std::ofstream f(tmp_playlist);
            if (f.is_open()) {
                f << local_file << "\n";
                f << online_url << "\n";
                f.close();
                
                LOG(fmt::format("[MPV] Seamless playlist created: {}", tmp_playlist));
                EVENT_LOG("Seamless: Playlist [local -> online]");
                
                const char* cmd[] = {"loadlist", tmp_playlist.c_str(), "replace", nullptr};
                int result = mpv_command(ctx_, cmd);
                LOG(fmt::format("[MPV] Seamless loadlist result: {}", result));
                
                if (result >= 0) {
                    EVENT_LOG("PRE-FETCHING: Online stream queued");
                }
            } else {
                // 回退到直接播放在线流
                LOG("[MPV] Seamless: Failed to create playlist, falling back to online");
                EVENT_LOG("Seamless: Fallback to online");
                play(online_url, is_video);
            }
        }
        
        //智能播放 - 自动检测本地缓存并选择最佳播放方式
        void play_smart(const std::string& url, const std::string& local_file, bool is_video = false) {
            // 如果有本地完整文件，直接播放
            if (!local_file.empty() && fs::exists(local_file)) {
                if (Utils::is_partial_download(local_file)) {
                    // 部分下载：无缝切换到在线流
                    LOG(fmt::format("[MPV] Smart: Partial cache, seamless play: {}", local_file));
                    EVENT_LOG(fmt::format("Cache Play: Partial -> Seamless ({})", fs::path(local_file).filename().string()));
                    play_seamless(local_file, url, is_video);
                } else {
                    // 完整下载：直接播放本地文件
                    LOG(fmt::format("[MPV] Smart: Full cache, local play: {}", local_file));
                    EVENT_LOG(fmt::format("Cache Play: Local ({})", fs::path(local_file).filename().string()));
                    play("file://" + local_file, is_video);
                }
            } else {
                // 无本地缓存：直接播放在线流
                LOG(fmt::format("[MPV] Smart: No cache, online play: {}", url));
                EVENT_LOG("Play: Online (no cache)");
                play(url, is_video);
            }
        }
        
        void play_list(const std::vector<std::string>& urls, bool is_video = false) {
            if (urls.empty()) return;

            //播放列表模式设置
            // keep-open=no 允许播放列表自动播放下一项
            // 这解决了播放完一个节目后停止的问题
            mpv_set_option_string(ctx_, "keep-open", "no");
            LOG("[MPV] Playlist mode: keep-open=no for auto-play next");

            if (is_video && Utils::has_gui()) {
                mpv_set_option_string(ctx_, "vo", "gpu");
                mpv_set_option_string(ctx_, "force-window", "yes");
            } else {
                mpv_set_option_string(ctx_, "force-window", "no");
                mpv_set_option_string(ctx_, "vo", "null");
            }

            std::string tmp = fmt::format("/tmp/podradio_{}.m3u", getpid());
            std::ofstream f(tmp);
            if (f.is_open()) {
                for (const auto& url : urls) f << url << "\n";
                f.close();
                const char* cmd[] = {"loadlist", tmp.c_str(), "replace", nullptr};
                mpv_command(ctx_, cmd);

                //确保播放开始（不暂停）
                int pause_val = 0;
                mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
                LOG("[MPV] play_list: Ensured playing (pause=no)");
            } else play(urls[0], is_video);
        }
        
        // V0.05B9n3e5g3c: 从指定位置开始播放播放列表
        // 用于L模式：完整播放列表，从指定索引开始播放
        void play_list_from(const std::vector<std::string>& urls, int start_idx, bool is_video = false) {
            if (urls.empty()) return;
            if (start_idx < 0) start_idx = 0;
            if (start_idx >= (int)urls.size()) start_idx = urls.size() - 1;

            //播放列表模式设置
            mpv_set_option_string(ctx_, "keep-open", "no");
            LOG(fmt::format("[MPV] Playlist mode: keep-open=no, start from idx {}", start_idx));

            if (is_video && Utils::has_gui()) {
                mpv_set_option_string(ctx_, "vo", "gpu");
                mpv_set_option_string(ctx_, "force-window", "yes");
            } else {
                mpv_set_option_string(ctx_, "force-window", "no");
                mpv_set_option_string(ctx_, "vo", "null");
            }

            std::string tmp = fmt::format("/tmp/podradio_{}.m3u", getpid());
            std::ofstream f(tmp);
            if (f.is_open()) {
                for (const auto& url : urls) f << url << "\n";
                f.close();
                
                // 加载播放列表
                const char* cmd[] = {"loadlist", tmp.c_str(), "replace", nullptr};
                mpv_command(ctx_, cmd);
                
                // 设置播放位置到指定的起始索引
                int64_t pos = start_idx;
                mpv_set_property(ctx_, "playlist-pos", MPV_FORMAT_INT64, &pos);
                LOG(fmt::format("[MPV] play_list_from: Set playlist-pos to {}", start_idx));

                //确保播放开始（不暂停）
                int pause_val = 0;
                mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &pause_val);
                LOG("[MPV] play_list_from: Ensured playing (pause=no)");
            } else play(urls[start_idx], is_video);
        }
        
        void toggle_pause() {
            int p = 0;
            mpv_get_property(ctx_, "pause", MPV_FORMAT_FLAG, &p);
            p = !p;
            mpv_set_property(ctx_, "pause", MPV_FORMAT_FLAG, &p);
        }
        
        void set_volume(int vol) {
            if (vol < 0) vol = 0;
            if (vol > MAX_VOLUME) vol = MAX_VOLUME;
            int64_t v = vol;
            mpv_set_property(ctx_, "volume", MPV_FORMAT_INT64, &v);
            state_.volume = vol;  //同步更新state_
        }
        
        void adjust_speed(bool faster) {
            double s = DEFAULT_SPEED;
            mpv_get_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &s);
            s = faster ? s * (1.0 + SPEED_STEP) : s / (1.0 + SPEED_STEP);
            if (s < MIN_SPEED) s = MIN_SPEED;
            if (s > MAX_SPEED) s = MAX_SPEED;
            mpv_set_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &s);
        }
        
        void reset_speed() {
            double s = DEFAULT_SPEED;
            mpv_set_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &s);
        }
        
        //直接设置播放速度
        void set_speed(double s) {
            if (s < MIN_SPEED) s = MIN_SPEED;
            if (s > MAX_SPEED) s = MAX_SPEED;
            mpv_set_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &s);
            state_.speed = s;
        }
        
        // V0.05B9n3e5g3c: 设置单曲循环（R模式）
        // loop=true时循环当前文件，loop=false时取消循环
        void set_loop_file(bool loop) {
            const char* val = loop ? "inf" : "no";
            mpv_set_option_string(ctx_, "loop-file", val);
            LOG(fmt::format("[MPV] loop-file set to: {}", val));
        }
        
        // V0.05B9n3e5g3c: 设置播放列表循环（C模式）
        // loop=true时循环整个播放列表，loop=false时取消循环
        void set_loop_playlist(bool loop) {
            const char* val = loop ? "inf" : "no";
            mpv_set_option_string(ctx_, "loop-playlist", val);
            LOG(fmt::format("[MPV] loop-playlist set to: {}", val));
        }
        
        struct State {
            bool paused = true, has_media = false, core_idle = true;
            int volume = MAX_VOLUME;
            double speed = DEFAULT_SPEED;
            double time_pos = 0.0;       // V2.39-FF: 当前播放位置
            double media_duration = 0.0; // V2.39-FF: 媒体总时长
            std::string title, current_url, audio_codec, video_codec;
            bool has_video = false;      // V2.39-FF: 是否有视频轨道（运行时检测）
            int playlist_pos = -1;       // V0.05B9n3e5g3b: 当前播放列表位置（0-based）
            int playlist_count = 0;      // V0.05B9n3e5g3b: 播放列表总数
        };
        
        State get_state() {
            std::lock_guard<std::mutex> lock(mtx_);
            return state_;
        }
        
        bool is_ytdl_available() const { return ytdl_available_; }
        mpv_handle* get_handle() { return ctx_; }
        
        //设置播放结束回调
        void set_end_file_callback(EndFileCallback callback) {
            end_file_callback_ = callback;
        }
        
    private:
        mpv_handle* ctx_ = nullptr;
        std::thread mpv_thread_;
        std::atomic<bool> running_{false};
        std::mutex mtx_;
        State state_;
        bool ytdl_available_ = false;
        std::string ytdl_path_;
        EndFileCallback end_file_callback_;  //播放结束回调
        
        void event_loop() {
            while (running_) {
                mpv_event* event = mpv_wait_event(ctx_, 0.05);
                if (event->event_id == MPV_EVENT_SHUTDOWN) break;
                
                if (event->event_id == MPV_EVENT_FILE_LOADED) {
                    LOG("[MPV] File loaded");
                    EVENT_LOG("MPV: File loaded");
                } else if (event->event_id == MPV_EVENT_END_FILE) {
                    mpv_event_end_file* ef = (mpv_event_end_file*)event->data;
                    int reason = static_cast<int>(ef->reason);
                    LOG(fmt::format("[MPV] End file, reason: {}", reason));
                    //MPV end-file reason详细记录
                    // 0 = EOF (正常结束，可能是无缝切换到下一曲)
                    // 1 = stop (停止)
                    // 2 = quit (退出)
                    // 3 = error (真正的错误)
                    // 4 = redirect
                    // 5 = 被新文件加载中断（正常，比如快速切换播放）
                    if (reason == 0) {
                        EVENT_LOG("MPV: Track ended (gapless next?)");
                    } else if (reason == 3) {
                        EVENT_LOG("MPV: Playback error!");
                    } else if (reason == 5) {
                        EVENT_LOG("MPV: Interrupted by new file");
                    }
                    
                    //调用播放结束回调
                    // reason=0时表示正常播放结束，触发自动播放下一曲逻辑
                    if (end_file_callback_) {
                        end_file_callback_(reason);
                    }
                } else if (event->event_id == MPV_EVENT_SEEK) {
                    LOG("[MPV] Seek event");
                } else if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
                    LOG("[MPV] Playback restart");
                    EVENT_LOG("MPV: Playback restart");
                }
                
                update_state();
            }
        }
        
        // V0.03a: 修复段错误 - 添加空指针检查
        // V2.39: 参考V2.26修复播放状态检测
        // V2.39-FF: 增加运行时视频检测
        void update_state() {
            // V0.03a: 空指针检查，防止段错误
            if (!ctx_) return;
            
            int p = 0;
            mpv_get_property(ctx_, "pause", MPV_FORMAT_FLAG, &p);
            
            char* path = nullptr;
            int has_path = mpv_get_property(ctx_, "path", MPV_FORMAT_STRING, &path);
            
            int64_t v = 100;
            mpv_get_property(ctx_, "volume", MPV_FORMAT_INT64, &v);
            
            double sp = 1.0;
            mpv_get_property(ctx_, "speed", MPV_FORMAT_DOUBLE, &sp);
            
            char* t = nullptr;
            mpv_get_property(ctx_, "media-title", MPV_FORMAT_STRING, &t);
            
            int idle = 1;
            mpv_get_property(ctx_, "core-idle", MPV_FORMAT_FLAG, &idle);
            
            char* codec = nullptr;
            mpv_get_property(ctx_, "audio-codec", MPV_FORMAT_STRING, &codec);
            
            char* vcodec = nullptr;
            mpv_get_property(ctx_, "video-codec", MPV_FORMAT_STRING, &vcodec);
            
            // V2.39-FF: 获取播放时间和时长
            double time_pos = 0.0;
            mpv_get_property(ctx_, "time-pos", MPV_FORMAT_DOUBLE, &time_pos);

            double duration = 0.0;
            mpv_get_property(ctx_, "duration", MPV_FORMAT_DOUBLE, &duration);

            // V0.05B9n3e5g3b: 获取播放列表位置和总数
            int64_t pl_pos = -1;
            mpv_get_property(ctx_, "playlist-pos", MPV_FORMAT_INT64, &pl_pos);

            int64_t pl_count = 0;
            mpv_get_property(ctx_, "playlist-count", MPV_FORMAT_INT64, &pl_count);

            {
                std::lock_guard<std::mutex> lock(mtx_);
                state_.paused = p;
                // V2.39: 关键修复 - 使用has_path >= 0 && path判断
                state_.has_media = (has_path >= 0 && path);
                state_.volume = (int)v;
                state_.speed = sp;
                state_.time_pos = time_pos;
                state_.media_duration = duration;
                state_.playlist_pos = (int)pl_pos;
                state_.playlist_count = (int)pl_count;
                if (t) state_.title = t;
                if (path) state_.current_url = path;
                state_.core_idle = idle;
                if (codec) state_.audio_codec = codec;
                if (vcodec) state_.video_codec = vcodec;
                // V2.39-FF: 运行时检测是否有视频轨道
                // 如果video-codec属性存在且非空，说明有视频流
                state_.has_video = (vcodec != nullptr && strlen(vcodec) > 0);
            }
            
            if (path) mpv_free(path);
            if (t) mpv_free(t);
            if (codec) mpv_free(codec);
            if (vcodec) mpv_free(vcodec);
        }
    };


    // =========================================================
    // DisplayItem
    // =========================================================
    struct DisplayItem {
        TreeNodePtr node;
        int depth;
        bool is_last;
        int parent_idx;
    };

    // =========================================================
    // 睡眠定时器 (V0.02新增，V0.05B9n1aa移动到此处)
    // =========================================================
    class SleepTimer {
    public:
        static SleepTimer& instance() { static SleepTimer st; return st; }

        void set_duration(int seconds) {
            duration_seconds_ = seconds;
            start_time_ = std::chrono::steady_clock::now();
            active_ = true;
            EVENT_LOG(fmt::format("Sleep timer set: {}s", seconds));
        }

        // 支持多种格式: "5h", "30m", "1:25:15", "90"
        void set_duration(const std::string& time_str) {
            int seconds = parse_time_string(time_str);
            if (seconds > 0) set_duration(seconds);
        }

        bool check_expired() {
            if (!active_ || duration_seconds_ <= 0) return false;
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
            if (elapsed >= duration_seconds_) {
                active_ = false;
                EVENT_LOG("Sleep timer expired!");
                return true;
            }
            return false;
        }

        int remaining_seconds() {
            if (!active_ || duration_seconds_ <= 0) return 0;
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
            int remaining = duration_seconds_ - elapsed;
            return remaining > 0 ? remaining : 0;
        }

        bool is_active() const { return active_; }
        void cancel() { active_ = false; EVENT_LOG("Sleep timer cancelled"); }

        // 静态解析函数，供main使用
        static int parse_time_string(const std::string& time_str) {
            int seconds = 0;
            std::string s = time_str;

            // 移除可能的负号前缀（如 -5h）
            if (!s.empty() && s[0] == '-') {
                s = s.substr(1);
            }

            // 检查后缀格式: 5h, 30m, 90s
            if (s.size() >= 2) {
                char suffix = std::tolower(s.back());
                std::string num_part = s.substr(0, s.size() - 1);
                try {
                    int val = std::stoi(num_part);
                    if (suffix == 'h') return val * 3600;
                    if (suffix == 'm') return val * 60;
                    if (suffix == 's') return val;
                } catch (...) {}
            }

            // 检查 HH:MM:SS 格式
            if (s.find(':') != std::string::npos) {
                std::vector<int> parts;
                std::stringstream ss(s);
                std::string part;
                while (std::getline(ss, part, ':')) {
                    try { parts.push_back(std::stoi(part)); }
                    catch (...) { parts.push_back(0); }
                }
                if (parts.size() >= 3) seconds = parts[0] * 3600 + parts[1] * 60 + parts[2];
                else if (parts.size() == 2) seconds = parts[0] * 60 + parts[1];
                else if (parts.size() == 1) seconds = parts[0];
                return seconds;
            }

            // 纯数字自动判断逻辑 (V0.05A2修正)
            // 规则：val < 100 → 小时，val >= 100 → 分钟
            try {
                int val = std::stoi(s);
                if (val >= 100) {
                    seconds = val * 60;   // >=100 视为分钟
                } else {
                    seconds = val * 3600; // <100 视为小时
                }
            } catch (...) {}

            return seconds;
        }

    private:
        SleepTimer() : duration_seconds_(0), active_(false) {}
        int duration_seconds_;
        std::chrono::steady_clock::time_point start_time_;
        bool active_;
    };

    // =========================================================
    //UILayout - 统一布局管理模块
    // =========================================================
    struct UILayout {
        int rows;
        int cols;
        int top_h;
        int bottom_h;
        int left_w;
        int right_w;
        int left_x;
        int right_x;
        
        static constexpr int BOTTOM_PANEL = 3;
        static constexpr int LEFT_PERCENT = 40;
        
        void update() {
            getmaxyx(stdscr, rows, cols);
            bottom_h = BOTTOM_PANEL;
            top_h = rows - bottom_h;
            left_w = (cols * LEFT_PERCENT) / 100;
            right_w = cols - left_w;
            left_x = 0;
            right_x = left_w;
        }
        
        void apply(WINDOW* left, WINDOW* right, WINDOW* status) {
            wresize(left, top_h, left_w);
            mvwin(left, 0, left_x);
            wresize(right, top_h, right_w);
            mvwin(right, 0, right_x);
            wresize(status, bottom_h, cols);
            mvwin(status, top_h, 0);
        }
        
        int right_inner_width() const { return right_w - 3; }
        int left_inner_width() const { return left_w - 2; }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // 统一边框绘制 - V0.05B9n3e5g3l优化
    // 功能：绘制完整的矩形边框，确保四角和四边完全闭合
    // ═══════════════════════════════════════════════════════════════════════════
    static void draw_box(WINDOW* win) {
        // 清空窗口内容
        werase(win);
        
        // 获取窗口尺寸
        int ww, wh;
        getmaxyx(win, wh, ww);
        
        // 边界保护：窗口尺寸必须至少2x2才能绘制边框
        if (ww < 2 || wh < 2) return;
        
        // ════ 绘制四条边（从角开始，确保闭合）════
        // 上边框：从左上角到右上角（不含角）
        for (int i = 1; i < ww - 1; i++) {
            mvwaddch(win, 0, i, ACS_HLINE);
        }
        // 下边框：从左下角到右下角（不含角）
        for (int i = 1; i < ww - 1; i++) {
            mvwaddch(win, wh - 1, i, ACS_HLINE);
        }
        // 左边框：从左上角到左下角（不含角）
        for (int i = 1; i < wh - 1; i++) {
            mvwaddch(win, i, 0, ACS_VLINE);
        }
        // 右边框：从右上角到右下角（不含角）
        for (int i = 1; i < wh - 1; i++) {
            mvwaddch(win, i, ww - 1, ACS_VLINE);
        }
        
        // ════ 绘制四个角（最后绘制，确保闭合）════
        mvwaddch(win, 0, 0, ACS_ULCORNER);       // ┌ 左上角
        mvwaddch(win, 0, ww - 1, ACS_URCORNER);  // ┐ 右上角
        mvwaddch(win, wh - 1, 0, ACS_LLCORNER);  // └ 左下角
        mvwaddch(win, wh - 1, ww - 1, ACS_LRCORNER); // ┘ 右下角
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // 边框保护函数 - V0.05B9n3e5g3l优化
    // 功能：强制重绘边框，覆盖任何溢出内容，确保四角四边完全闭合
    // 参数：
    //   win: 窗口指针
    //   ww, wh: 窗口宽度和高度
    //   title_start, title_end: 标题嵌入区域（列索引），-1表示无嵌入
    //   split_y: 分隔线行号，-1表示无分隔线
    //   split_title_start, split_title_end: 分隔线标题区域
    // ═══════════════════════════════════════════════════════════════════════════
    static void protect_border(WINDOW* win, int ww, int wh,
                               int title_start = -1, int title_end = -1,
                               int split_y = -1,
                               int split_title_start = -1, int split_title_end = -1) {
        
        // 边界保护：窗口尺寸必须至少2x2
        if (ww < 2 || wh < 2) return;
        
        // ════ 第一步：绘制四条边 ════
        
        // ===== 上边框（跳过标题嵌入区域）=====
        if (title_start < 0) {
            // 无嵌入，整条重绘（不含角）
            for (int i = 1; i < ww - 1; i++) {
                mvwaddch(win, 0, i, ACS_HLINE);
            }
        } else {
            // 有嵌入，分段重绘：左边部分 + 右边部分
            for (int i = 1; i < title_start; i++) {
                mvwaddch(win, 0, i, ACS_HLINE);
            }
            for (int i = title_end; i < ww - 1; i++) {
                mvwaddch(win, 0, i, ACS_HLINE);
            }
        }
        
        // ===== 下边框（整条重绘，不含角）=====
        for (int i = 1; i < ww - 1; i++) {
            mvwaddch(win, wh - 1, i, ACS_HLINE);
        }
        
        // ===== 左右边框（完整重绘，不含角）=====
        for (int i = 1; i < wh - 1; i++) {
            mvwaddch(win, i, 0, ACS_VLINE);
            mvwaddch(win, i, ww - 1, ACS_VLINE);
        }
        
        // ════ 第二步：绘制四个角（最后绘制，确保闭合）════
        mvwaddch(win, 0, 0, ACS_ULCORNER);       // ┌ 左上角
        mvwaddch(win, 0, ww - 1, ACS_URCORNER);  // ┐ 右上角
        mvwaddch(win, wh - 1, 0, ACS_LLCORNER);  // └ 左下角
        mvwaddch(win, wh - 1, ww - 1, ACS_LRCORNER); // ┘ 右下角
        
        // ════ 第三步：分隔线（如有）════
        if (split_y > 0 && split_y < wh - 1) {
            // 先绘制左右连接符
            mvwaddch(win, split_y, 0, ACS_LTEE);    // ├ 左连接
            mvwaddch(win, split_y, ww - 1, ACS_RTEE); // ┤ 右连接
            
            // 然后绘制分隔线水平部分
            if (split_title_start < 0) {
                for (int i = 1; i < ww - 1; i++) {
                    mvwaddch(win, split_y, i, ACS_HLINE);
                }
            } else {
                for (int i = 1; i < split_title_start; i++) {
                    mvwaddch(win, split_y, i, ACS_HLINE);
                }
                for (int i = split_title_end; i < ww - 1; i++) {
                    mvwaddch(win, split_y, i, ACS_HLINE);
                }
            }
        }
    }

    // =========================================================
    // UI渲染 - V2.39: 修复滚动显示和右对齐
    // =========================================================
    class UI {
    public:
        void init(float ratio = 0.4f) {
            //在任何终端操作之前，立即获取终端大小
            // 这是解决SSH/TTY环境UI大小问题的关键
            int term_rows = 0, term_cols = 0;
            struct winsize ws;
            if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
                term_rows = ws.ws_row;
                term_cols = ws.ws_col;
            }
            // 备用方案：从环境变量获取
            if (term_rows <= 0 || term_cols <= 0) {
                const char* lines_env = std::getenv("LINES");
                const char* cols_env = std::getenv("COLUMNS");
                if (lines_env) term_rows = std::atoi(lines_env);
                if (cols_env) term_cols = std::atoi(cols_env);
            }
            // 如果全局保存的值更大，使用全局值
            if (g_original_lines > term_rows) term_rows = g_original_lines;
            if (g_original_cols > term_cols) term_cols = g_original_cols;
            
            //设置locale（不打印到终端，仅记录到日志）
            // 关键修复：不在setlocale前后进行dup2重定向，避免破坏UTF-8环境
            char* locale_result = setlocale(LC_ALL, "");
            if (locale_result) {
                LOG(fmt::format("[INIT] Locale set to: {}", locale_result));
            }
            
            //初始化ncurses
            initscr();
            g_ncurses_initialized = true;
            
            //强制设置正确的终端大小
            if (term_rows > 0 && term_cols > 0) {
                resizeterm(term_rows, term_cols);
                LINES = term_rows;
                COLS = term_cols;
            }
            wrefresh(stdscr);
            
            cbreak();
            noecho();
            keypad(stdscr, TRUE);
            curs_set(0);
            timeout(50);  //50ms降低CPU占用（20FPS）
            start_color();
            use_default_colors();
            
            for (int i = 0; i < 256; ++i) init_pair(i + 1, i, -1);
            
            init_pair(10, COLOR_CYAN, COLOR_BLACK);
            init_pair(11, COLOR_GREEN, COLOR_BLACK);
            init_pair(12, COLOR_YELLOW, COLOR_BLACK);
            init_pair(13, COLOR_RED, COLOR_BLACK);
            init_pair(14, COLOR_BLUE, COLOR_BLACK);
            init_pair(15, COLOR_MAGENTA, COLOR_BLACK);
            
            getmaxyx(stdscr, h, w);
            //使用整数运算避免浮点精度问题
            layout_ratio_ = ratio;
            left_w = w * 40 / 100;  // 40%固定比例，整数运算
            right_w = w - left_w;
            top_h = h - 3;
            
            // 边界保护：确保最小窗口尺寸
            if (left_w < 10) left_w = 10;
            if (right_w < 10) right_w = 10;
            if (top_h < 5) top_h = 5;
            
            left_win = newwin(top_h, left_w, 0, 0);
            right_win = newwin(top_h, right_w, 0, left_w);
            status_win = newwin(3, w, top_h, 0);
            
            //空指针检查，防止崩溃
            if (!left_win || !right_win || !status_win) {
                endwin();
                fprintf(stderr, "Error: Failed to create windows\n");
                exit(1);
            }
            
            //禁用窗口自动换行和滚动，防止内容溢出边界
            scrollok(left_win, FALSE);
            scrollok(right_win, FALSE);
            scrollok(status_win, FALSE);
            immedok(left_win, FALSE);
            immedok(right_win, FALSE);
            immedok(status_win, FALSE);
            
            statusbar_color_config_ = IniConfig::instance().get_statusbar_color_config();
            show_tree_lines_ = IniConfig::instance().get_bool("display", "show_tree_lines", true);
            scroll_mode_ = IniConfig::instance().get_bool("display", "scroll_mode", false);  //默认false
            
            CacheManager::instance().load();
        }
        
        void cleanup() {
            delwin(left_win);
            delwin(right_win);
            delwin(status_win);
            //使用全局清理函数，确保恢复光标和终端属性
            tui_cleanup();
        }
        
        void toggle_tree_lines() {
            show_tree_lines_ = !show_tree_lines_;
            EVENT_LOG(fmt::format("Tree lines: {}", show_tree_lines_ ? "ON" : "OFF"));
        }
        
        void toggle_scroll_mode() {
            scroll_mode_ = !scroll_mode_;
            EVENT_LOG(fmt::format("Scroll mode: {}", scroll_mode_ ? "ON" : "OFF"));
        }
        
        void set_scroll_mode(bool mode) { scroll_mode_ = mode; }
        void set_show_tree_lines(bool show) { show_tree_lines_ = show; }
        
        bool is_scroll_mode() const { return scroll_mode_; }

        void draw(AppMode mode, const std::vector<DisplayItem>& list, int selected,
                  const MPVController::State& state, int view_start,
                  AppState app_state, TreeNodePtr playback_node, int marked_count,
                  const std::string& search_query, int current_match, int total_matches,
                  TreeNodePtr selected_node, const std::vector<DownloadProgress>& downloads,
                  bool visual_mode, int visual_start,
                  const std::vector<PlaylistItem>& playlist = {}, int playlist_index = -1,
                  //List模式参数
                  const std::vector<SavedPlaylistItem>& saved_playlist = {}, int list_selected_idx = 0,
                  //List模式标记索引
                  const std::set<int>& list_marked_indices = {},
                  //播放模式
                  PlayMode play_mode = PlayMode::SINGLE) {
            getmaxyx(stdscr, h, w);
            //使用整数运算避免浮点精度问题
            // 布局比例40%，使用整数乘除法
            int new_left_w = w * 40 / 100;
            int new_right_w = w - new_left_w;
            int new_top_h = h - 3;
            
            //检测窗口尺寸是否变化
            bool size_changed = (new_left_w != left_w || new_right_w != right_w || new_top_h != top_h || w != cols_);
            
            if (size_changed) {
                //窗口尺寸变化时，先清除所有窗口内容
                // 这样可以避免旧的边框字符残留在屏幕上
                werase(left_win);
                werase(right_win);
                werase(status_win);
                werase(stdscr);
                
                // 更新缓存的尺寸
                left_w = new_left_w;
                right_w = new_right_w;
                top_h = new_top_h;
                cols_ = w;
            }
            
            wresize(left_win, top_h, left_w);
            wresize(right_win, top_h, right_w);
            mvwin(right_win, 0, left_w);
            wresize(status_win, 3, w);
            mvwin(status_win, top_h, 0);

            //更保守计算标题栏净宽，保护右上角边框
            // 右上角需要保留：┐ 和 ─ 共2列
            int title_max = left_w - 6; // 左边框1 + 空格1 + 标题 + 空格1 + 右上角保护3
            if (title_max < 5) title_max = 5;  // 最小保留
            
            std::string mode_str;
            switch (mode) {
                case AppMode::RADIO: mode_str = "📻 RADIO"; break;
                case AppMode::PODCAST: mode_str = "🎙️ PODCAST"; break;
                case AppMode::FAVOURITE: mode_str = "💖 FAVOURITE"; break;
                case AppMode::HISTORY: mode_str = "📜 HISTORY"; break;  //新增
                case AppMode::ONLINE: mode_str = fmt::format("🔍 ONLINE [{}]", 
                    ITunesSearch::get_region_name(OnlineState::instance().current_region)); break;  //新增
            }
            if (visual_mode) mode_str = "§ VISUAL §";
            if (marked_count > 0) mode_str += " [🔖 x" + std::to_string(marked_count) + "]";  //新格式
            // V2.39-FF: 添加T和S状态指示器
            if (show_tree_lines_) mode_str += " [T]";
            if (scroll_mode_) mode_str += " [S]";
            
            std::string title_display = Utils::truncate_by_display_width(mode_str, title_max);
            
            //增量重绘 - 不在这里调用draw_box
            
            //窗口结构（box()后）：
            //   列0 = 左边框 │
            //   列1 到 列left_w-2 = 内容区（宽度=left_w-2）
            //   列left_w-1 = 右边框 │

            //充分利用内容区宽度
            //   内容区宽度 = left_w - 2
            //   不再右边留空格保护，内容紧贴右边框
            //   tree_line_width = left_w - 2

            // 注意：内容从列1开始打印
            int tree_line_width = left_w - 2;
            if (tree_line_width < 10) tree_line_width = 10;  // 最小宽度保护

            // ═══════════════════════════════════════════════════════════════════
            //List模式改为居中弹窗显示
            //采用box()模式，标题内部居中，三角嵌入边框
            // ═══════════════════════════════════════════════════════════════════
            if (app_state == AppState::LIST_MODE) {
                // 先绘制正常的主界面（三个矩形边框）
                draw_box(left_win);
                mvwprintw(left_win, 0, 2, " %s ", title_display.c_str());
                mvwaddch(left_win, 0, left_w - 1, ACS_URCORNER);
                mvwaddch(left_win, 0, left_w - 2, ACS_HLINE);

                int line_num = 1;
                // vh 预留给未来功能：可变高度列表显示
                // int vh = top_h - 2;
                for (int i = view_start; i < (int)list.size() && line_num < top_h - 1; ++i) {
                    bool is_sel = (i == selected);
                    bool in_visual = visual_mode && visual_start >= 0 &&
                        ((visual_start <= i && i <= selected) || (selected <= i && i <= visual_start));
                    draw_line(left_win, line_num, list[i], is_sel, in_visual, tree_line_width, state.current_url);
                    line_num++;
                }

                // V0.05B9n3e5g3h: 确保右面板和状态栏边框完整绘制
                draw_box(right_win);
                mvwprintw(right_win, 0, 2, " INFO & LOG ");
                // 绘制右侧面板和状态栏
                // V0.05B9n3e5g3m: 添加saved_playlist参数以同步显示
                draw_info(right_win, state, AppState::BROWSING, playback_node, marked_count, search_query,
                          current_match, total_matches, selected_node, downloads, visual_mode, right_w - 3,
                          playlist, playlist_index, saved_playlist, list_selected_idx);
                draw_box(status_win);
                draw_status(status_win, state, selected_node);
                wnoutrefresh(left_win);
                wnoutrefresh(right_win);
                wnoutrefresh(status_win);

                // ═════════════════════════════════════════════════════════════
                //绘制居中弹窗（box模式）
                // ═════════════════════════════════════════════════════════════
                int popup_h = std::min(28, h * 7 / 10);  // 最大28行，或屏幕70%
                int popup_w = std::max(70, w * 8 / 10);  // 最小70列，或屏幕80%
                int popup_y = (h - popup_h) / 2;
                int popup_x = (w - popup_w) / 2;

                WINDOW* popup = newwin(popup_h, popup_w, popup_y, popup_x);
                keypad(popup, TRUE);

                // ═════════════════════════════════════════════════════════════
                // 绘制简单边框
                // ═════════════════════════════════════════════════════════════
                box(popup, 0, 0);

                // ═════════════════════════════════════════════════════════════
                // 标题居中显示在第一行内部
                // ═════════════════════════════════════════════════════════════
                // 获取播放模式图标
                std::string mode_icon;
                std::string mode_name;
                switch (play_mode) {
                    case PlayMode::SINGLE: mode_icon = "🔂"; mode_name = "单次"; break;
                    case PlayMode::CYCLE: mode_icon = "🔁"; mode_name = "循环"; break;
                    case PlayMode::RANDOM: mode_icon = "🔀"; mode_name = "随机"; break;
                }
                
                std::string popup_title = fmt::format("📋 PLAYLIST ({}) {}{}", 
                    saved_playlist.size(), mode_icon, mode_name);
                int title_w = Utils::utf8_display_width(popup_title);
                int title_x = (popup_w - title_w) / 2;
                if (title_x < 1) title_x = 1;
                
                wattron(popup, A_BOLD);
                mvwprintw(popup, 0, title_x, "%s", popup_title.c_str());
                wattroff(popup, A_BOLD);

                // ═════════════════════════════════════════════════════════════
                //可见行计算
                // 窗口结构：行0(顶边框+标题) + 内容区 + 行popup_h-4(分隔线) + 
                //          行popup_h-3(热键1) + 行popup_h-2(热键2) + 行popup_h-1(底边框)
                // 可见内容行 = popup_h - 5
                // ═════════════════════════════════════════════════════════════
                int visible_lines = popup_h - 5;
                if (visible_lines < 1) visible_lines = 1;

                // 计算滚动偏移
                int scroll_off = 0;
                if (list_selected_idx >= visible_lines - 1) {
                    scroll_off = list_selected_idx - visible_lines + 2;
                }

                int above_count = scroll_off;
                int below_count = std::max(0, static_cast<int>(saved_playlist.size()) - scroll_off - visible_lines);

                // ═════════════════════════════════════════════════════════════
                // 右上角嵌入▲指示器
                // ═════════════════════════════════════════════════════════════
                if (above_count > 0) {
                    mvwprintw(popup, 0, popup_w - 3, "▲");
                }

                // ═════════════════════════════════════════════════════════════
                // 绘制内容区域
                // ═════════════════════════════════════════════════════════════
                int content_y = 1;
                int content_w = popup_w - 2;  // 内容宽度

                // 当前播放的URL（用于标记正在播放的项）
                std::string playing_url = state.has_media ? state.current_url : "";

                for (size_t i = scroll_off; i < saved_playlist.size() && content_y < popup_h - 4; ++i) {
                    const auto& item = saved_playlist[i];
                    bool is_selected = (static_cast<int>(i) == list_selected_idx);
                    bool is_playing = (item.url == playing_url);

                    // ═════════════════════════════════════════════════════════════
                    //统一固定宽度格式
                    // 格式: "序号. 标题 状态标记"
                    // 序号固定4字符宽度(如"1.  "或"10. ")
                    // 播放状态标记在标题右侧
                    // ═════════════════════════════════════════════════════════════
                    
                    // 检查是否失效
                    bool is_invalid = item.url.empty();
                    
                    // 构建序号部分 (固定4字符宽度)
                    std::string num_part = std::to_string(i + 1) + ". ";
                    while (num_part.length() < 4) num_part += " ";
                    if (num_part.length() > 4) num_part = num_part.substr(0, 4);
                    
                    // 状态标记 (在右侧)
                    // V0.05B9n3e5g3l: 删除 ◀ 选中标记，选中项已用反白显示，无需额外标记
                    std::string status_mark;
                    if (is_invalid) {
                        status_mark = " ⚠";  // 失效
                    } else if (is_playing) {
                        status_mark = " 🔊";  // 正在播放
                    }
                    
                    // 标记图标 (在序号前)
                    std::string mark_prefix;
                    if (list_marked_indices.count(static_cast<int>(i))) {
                        mark_prefix = "🔖 ";
                        num_part = "";  // 有标记时去掉序号，保持对齐
                    }

                    // 计算标题可用宽度
                    int title_max_w = content_w - 4 - Utils::utf8_display_width(status_mark) - Utils::utf8_display_width(mark_prefix);
                    if (title_max_w < 10) title_max_w = 10;
                    
                    // 截断标题
                    std::string truncated_title = Utils::truncate_by_display_width(item.title, title_max_w);
                    
                    // 构建完整行
                    std::string line = mark_prefix + num_part + truncated_title + status_mark;

                    // 高亮显示
                    if (is_selected) {
                        wattron(popup, A_REVERSE);
                    } else if (is_playing) {
                        wattron(popup, COLOR_PAIR(11));  // 绿色高亮
                    } else if (is_invalid) {
                        wattron(popup, COLOR_PAIR(13) | A_DIM);  // 红色暗淡
                    }

                    // 截断并显示
                    std::string truncated = Utils::truncate_by_display_width(line, content_w);
                    mvwprintw(popup, content_y, 1, "%s", truncated.c_str());

                    // 清除该行剩余部分
                    int line_w = Utils::utf8_display_width(truncated);
                    for (int j = line_w + 1; j < content_w; ++j) {
                        waddch(popup, ' ');
                    }

                    if (is_selected) {
                        wattroff(popup, A_REVERSE);
                    } else if (is_playing) {
                        wattroff(popup, COLOR_PAIR(11));
                    } else if (is_invalid) {
                        wattroff(popup, COLOR_PAIR(13) | A_DIM);
                    }

                    content_y++;
                }

                // 填充空白行
                while (content_y < popup_h - 4) {
                    for (int j = 1; j < popup_w - 1; ++j) {
                        mvwaddch(popup, content_y, j, ' ');
                    }
                    content_y++;
                }

                // ═════════════════════════════════════════════════════════════
                // 绘制分隔线（在popup_h - 4位置，留出3行给热键+底边框）
                // ═════════════════════════════════════════════════════════════
                mvwaddch(popup, popup_h - 4, 0, ACS_LTEE);
                for (int i = 1; i < popup_w - 1; ++i) waddch(popup, ACS_HLINE);
                mvwaddch(popup, popup_h - 4, popup_w - 1, ACS_RTEE);

                // ═════════════════════════════════════════════════════════════
                // 第一行热键：操作热键（居中）- 在popup_h - 3位置
                // ═════════════════════════════════════════════════════════════
                std::string help_line1 = "[L]Close [j/k]Nav [g/G]Top/End [d]Del [J/K]Move [l/Enter]Play [+]Add";
                int help1_w = Utils::utf8_display_width(help_line1);
                int help1_x = (popup_w - help1_w) / 2;
                if (help1_x < 1) help1_x = 1;
                mvwprintw(popup, popup_h - 3, help1_x, "%s", help_line1.c_str());

                // ═════════════════════════════════════════════════════════════
                // 第二行热键：模式热键（居中）- 在popup_h - 2位置
                // popup_h - 1 留给box()的底边框
                // ═════════════════════════════════════════════════════════════
                std::string help_line2 = "[r]🔂Single [c]🔁Cycle [y]🔀Random [C]ClearAll";
                int help2_w = Utils::utf8_display_width(help_line2);
                int help2_x = (popup_w - help2_w) / 2;
                if (help2_x < 1) help2_x = 1;
                mvwprintw(popup, popup_h - 2, help2_x, "%s", help_line2.c_str());

                // 右下角嵌入▼指示器（在底边框内部）
                if (below_count > 0) {
                    mvwprintw(popup, popup_h - 2, popup_w - 3, "▼");
                }

                // 刷新弹窗
                wnoutrefresh(popup);
                doupdate();

                // 删除弹窗（下次重绘时重新创建）
                delwin(popup);

                return;  // List模式绘制完成
            }

            //增量重绘优化（简化版：检查边界条件）
            bool full_redraw = needs_full_redraw(selected, list.size(), view_start, mode);
            
            // 边界保护：列表为空或selected无效时强制完全重绘
            if (list.empty() || selected < 0 || selected >= (int)list.size()) {
                full_redraw = true;
            }
            
            // V0.05B9n3e5g3h: 窗口尺寸变化时强制完全重绘，确保边框闭合
            if (size_changed) {
                full_redraw = true;
            }
            
            int line_num = 1;
            // vh 预留给未来功能：可变高度列表显示
            // int vh = top_h - 2;
            
            if (full_redraw) {
                // 完全重绘：清空窗口并绘制所有行
                draw_box(left_win);
                // 重绘标题（draw_box会清空标题）
                mvwprintw(left_win, 0, 2, " %s ", title_display.c_str());
                mvwaddch(left_win, 0, left_w - 1, ACS_URCORNER);
                mvwaddch(left_win, 0, left_w - 2, ACS_HLINE);
                
                for (int i = view_start; i < (int)list.size() && line_num < top_h - 1; ++i) {
                    bool is_sel = (i == selected);
                    bool in_visual = visual_mode && visual_start >= 0 && 
                        ((visual_start <= i && i <= selected) || (selected <= i && i <= visual_start));
                    draw_line(left_win, line_num, list[i], is_sel, in_visual, tree_line_width, state.current_url);
                    line_num++;
                }
            } else {
                // 增量重绘：只重绘光标变化的行
                int old_screen_row = last_selected_idx_ - view_start + 1;
                int new_screen_row = selected - view_start + 1;
                
                // 重绘旧行（取消高亮）- 带完整边界检查
                if (old_screen_row >= 1 && old_screen_row < top_h - 1 && 
                    last_selected_idx_ >= 0 && last_selected_idx_ < (int)list.size()) {
                    bool in_visual_old = visual_mode && visual_start >= 0 &&
                        ((visual_start <= last_selected_idx_ && last_selected_idx_ <= selected) || 
                         (selected <= last_selected_idx_ && last_selected_idx_ <= visual_start));
                    draw_line(left_win, old_screen_row, list[last_selected_idx_], false, in_visual_old, tree_line_width, state.current_url);
                }
                
                // 重绘新行（高亮）- 带完整边界检查
                if (new_screen_row >= 1 && new_screen_row < top_h - 1 && 
                    selected >= 0 && selected < (int)list.size()) {
                    // visual模式下，当前选中项总是高亮
                    bool in_visual_new = visual_mode && visual_start >= 0;
                    draw_line(left_win, new_screen_row, list[selected], true, in_visual_new, tree_line_width, state.current_url);
                }
            }
            
            // 更新追踪状态
            update_redraw_state(selected, list.size(), view_start, mode);
            
            //左面板边框保护
            // 标题嵌入区域：列2 到 列2+title_display宽度+2
            int left_title_end = 2 + Utils::utf8_display_width(title_display) + 2;
            protect_border(left_win, left_w, top_h, 2, left_title_end);
            wnoutrefresh(left_win);  //双缓冲优化

            // Right Panel
            draw_box(right_win);  //使用统一边框绘制
            mvwprintw(right_win, 0, 2, " INFO & LOG ");
            
            //关键修复 - 使用getmaxx获取实际窗口宽度
            // 避免缓存变量与实际窗口尺寸不同步导致的边框缺口
            // 右面板结构：列0=左边框，列1~right_w-2=内容区，列right_w-1=右边框
            // 内容区宽度 = window_width - 3
            int right_inner_width = getmaxx(right_win) - 3;
            
            // V2.39-FF: 帮助现在使用独立弹窗显示，不再在此处判断
            //传递播放列表数据
            // V0.05B9n3e5g3m: 添加saved_playlist参数以同步显示
            draw_info(right_win, state, app_state, playback_node, marked_count,
                      search_query, current_match, total_matches, selected_node, downloads, 
                      visual_mode, right_inner_width, playlist, playlist_index,
                      saved_playlist, list_selected_idx);
            
            //右面板边框保护
            // 标题 " INFO & LOG " 嵌入区域：列2 到 列13
            protect_border(right_win, right_w, top_h, 2, 13);
            wnoutrefresh(right_win);  //双缓冲优化
            
            // Status Bar
            draw_status(status_win, state, selected_node);
            wnoutrefresh(status_win);  //双缓冲优化
            
            //双缓冲刷新 - 一次性更新所有窗口
            doupdate();
        }
        
        //输入取消标记（使用字符串连接避免十六进制转义问题）
        static constexpr const char* INPUT_CANCELLED = "\x01" "CANCELLED" "\x01";  // 特殊标记表示用户取消
        
        //修复UTF-8中文/CJK输入 + URL自适应显示
        // 参考V0.05B9n3d版本：完整的中文/CJK输入法支持
        //改进：窗口宽度自适应URL长度，长URL自动截断
        std::string input_box(const std::string& prompt, const std::string& default_val = "") {
            //开启回显和光标，让终端处理输入法显示
            echo();
            curs_set(1);
            
            //计算窗口宽度，自适应URL长度
            int prompt_w = Utils::utf8_display_width(prompt);
            int default_w = default_val.empty() ? 0 : Utils::utf8_display_width(default_val) + 10;  // "Default: "前缀
            int min_w = 50;
            int iw = std::max({prompt_w + 20, default_w + 4, min_w});
            
            // 限制最大宽度为屏幕宽度-4
            int max_w = w - 4;
            if (iw > max_w) iw = max_w;
            
            //如果default_val太长，需要增加窗口高度显示完整URL
            int ih = 5;  // 默认5行
            int default_display_w = default_val.empty() ? 0 : Utils::utf8_display_width(default_val);
            int available_w = iw - 6;  // 可用显示宽度（减去边框和前缀）
            
            if (default_display_w > available_w) {
                // URL太长，需要增加一行显示，或截断
                // 方案1：增加窗口高度显示完整URL（但限制最多7行）
                // 方案2：截断URL显示
                // 这里采用截断方案，保持窗口简洁
                // url_truncated = true; // 保留：未来可用于UI提示
            }
            
            int iy = h / 2 - ih / 2, ix = (w - iw) / 2;
            
            WINDOW* inp = newwin(ih, iw, iy, ix);
            keypad(inp, TRUE);
            box(inp, 0, 0);
            
            //根据 prompt 确定标题
            std::string title;
            std::string lower_prompt = prompt;
            std::transform(lower_prompt.begin(), lower_prompt.end(), lower_prompt.begin(), ::tolower);
            if (lower_prompt.find("search") != std::string::npos) {
                title = " SEARCH ";
            } else if (lower_prompt.find("title") != std::string::npos) {
                title = " EDIT TITLE ";
            } else if (lower_prompt.find("url") != std::string::npos) {
                title = " EDIT URL ";
            } else {
                title = " " + prompt + " ";
            }
            
            // 第0行：标题居中
            int title_display_w = Utils::utf8_display_width(title);
            int title_x = (iw - title_display_w) / 2;
            if (title_x < 1) title_x = 1;
            mvwprintw(inp, 0, title_x, "%s", title.c_str());
            
            // 第1行：Default 居中显示（V0.05B9n3e5g2RF2: 长URL截断处理）
            if (!default_val.empty()) {
                // 清空第1行
                wmove(inp, 1, 1);
                for (int i = 1; i < iw - 1; i++) {
                    waddch(inp, ' ');
                }
                
                // 计算可用宽度
                int prefix_w = 10;  // "Default: " 宽度
                int content_available = iw - 4;  // 总可用宽度（减去边框）
                
                std::string display_text;
                if (default_display_w <= content_available - prefix_w) {
                    // 完整显示
                    display_text = "Default: " + default_val;
                } else {
                    //URL太长，截断显示，添加...提示
                    int max_url_w = content_available - prefix_w - 3;  // 留3个字符给"..."
                    std::string truncated_url = Utils::truncate_by_display_width(default_val, max_url_w);
                    display_text = "Default: " + truncated_url + "...";
                }
                
                int display_w = Utils::utf8_display_width(display_text);
                int display_x = (iw - display_w) / 2;
                if (display_x < 1) display_x = 1;
                
                // 确保不超出边框
                if (display_x + display_w > iw - 2) {
                    display_x = 2;
                }
                mvwprintw(inp, 1, display_x, "%s", display_text.c_str());
            }
            
            // 第3行：按钮居中显示
            std::string btn_enter = "[Enter]Confirm";
            std::string btn_esc = "[ESC]Cancel";
            int left_margin = iw / 6;
            int middle_gap = iw / 3;
            int btn_enter_x = left_margin;
            int btn_esc_x = left_margin + static_cast<int>(btn_enter.length()) + middle_gap;
            
            if (btn_esc_x + static_cast<int>(btn_esc.length()) > iw - 2) {
                btn_enter_x = 2;
                btn_esc_x = iw - static_cast<int>(btn_esc.length()) - 2;
            }
            
            mvwprintw(inp, 3, btn_enter_x, "%s", btn_enter.c_str());
            mvwprintw(inp, 3, btn_esc_x, "%s", btn_esc.c_str());
            
            wrefresh(inp);
            
            //输入缓冲区
            std::string input;
            int max_input = iw - 4;
            
            // 更新输入行显示（居中）
            auto update_input_display = [&]() {
                // 清空第2行
                wmove(inp, 2, 1);
                for (int i = 1; i < iw - 1; i++) {
                    waddch(inp, ' ');
                }
                
                if (!input.empty()) {
                    // 计算输入内容的显示宽度
                    int display_width = Utils::utf8_display_width(input);
                    // 居中位置
                    int input_x = (iw - display_width) / 2;
                    if (input_x < 2) input_x = 2;
                    
                    //确保不超出边框
                    if (input_x + display_width > iw - 2) {
                        // 输入内容太长，截断显示
                        std::string truncated = Utils::truncate_by_display_width(input, iw - 4);
                        display_width = Utils::utf8_display_width(truncated);
                        input_x = 2;
                        mvwprintw(inp, 2, input_x, "%s", truncated.c_str());
                    } else {
                        mvwprintw(inp, 2, input_x, "%s", input.c_str());
                    }
                    
                    // 光标位置：内容后面
                    wmove(inp, 2, input_x + display_width);
                } else {
                    // 空输入时，光标居中
                    wmove(inp, 2, iw / 2);
                }
                wrefresh(inp);
            };
            
            // 初始显示
            update_input_display();
            
            while (true) {
                int ch = wgetch(inp);
                
                if (ch == '\n' || ch == KEY_ENTER) {
                    break;  // 确认输入
                } else if (ch == 27) {  // ESC键
                    //检测是否是真正的ESC取消
                    // 输入法可能会发送ESC序列，需要判断
                    nodelay(inp, TRUE);
                    int next = wgetch(inp);
                    nodelay(inp, FALSE);
                    
                    if (next == ERR) {
                        // 真正的ESC键（无后续字节），取消输入
                        delwin(inp);
                        curs_set(0);
                        noecho();
                        return INPUT_CANCELLED;
                    } else {
                        // 输入法序列的一部分，放回继续处理
                        ungetch(next);
                    }
                } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                    //退格删除 - 正确处理UTF-8多字节字符
                    if (!input.empty()) {
                        // 找到最后一个UTF-8字符的起始位置
                        size_t pos = input.length() - 1;
                        while (pos > 0 && (input[pos] & 0xC0) == 0x80) {
                            pos--;
                        }
                        // 从输入中删除最后一个字符
                        input.erase(pos);
                        update_input_display();
                    }
                } else if (ch == 21) {  // CTRL+U清空全部输入
                    if (!input.empty()) {
                        input.clear();
                        update_input_display();
                    }
                } else if (ch >= 32 && ch < 127) {
                    //ASCII字符，直接添加
                    if (input.length() < (size_t)(max_input - 2)) {
                        input += (char)ch;
                        update_input_display();
                    }
                } else if (ch >= 0x80) {
                    //UTF-8字符，读取完整字符
                    // 确定UTF-8字符的字节数
                    int expected = 0;
                    if ((ch & 0xE0) == 0xC0) expected = 1;
                    else if ((ch & 0xF0) == 0xE0) expected = 2;  // 中文属于此类
                    else if ((ch & 0xF8) == 0xF0) expected = 3;
                    
                    // 添加第一个字节
                    if (input.length() + expected + 1 < (size_t)(max_input - 2)) {
                        input += (char)ch;
                        
                        // 阻塞读取后续字节（确保不丢失）
                        for (int i = 0; i < expected; i++) {
                            int next = wgetch(inp);
                            if (next != ERR && next > 0) {
                                input += (char)next;
                            }
                        }
                        update_input_display();
                    }
                }
            }
            
            delwin(inp);
            curs_set(0);
            noecho();
            
            //如果用户输入为空，返回默认值
            return input.empty() ? default_val : input;
        }
        
        //检查是否为取消标记
        static bool is_input_cancelled(const std::string& result) {
            return result == INPUT_CANCELLED;
        }
        
        std::string dialog(const std::string& msg) {
            echo();
            curs_set(1);
            
            int ih = 3, iw = std::min((int)msg.length() + 10, w - 10);
            int iy = h / 2 - 1, ix = (w - iw) / 2;
            
            WINDOW* dlg = newwin(ih, iw, iy, ix);
            box(dlg, 0, 0);
            mvwprintw(dlg, 1, 2, "%s", msg.c_str());
            wrefresh(dlg);
            
            char buf[16] = {0};
            mvwgetnstr(dlg, 1, msg.length() + 3, buf, sizeof(buf) - 1);
            
            delwin(dlg);
            noecho();
            curs_set(0);
            
            return std::string(buf);
        }
        
        //Y/N确认对话框
        //YES/NO分布在下部两侧
        //标题显示在边框上横线中间，框内不重复显示
        bool confirm_box(const std::string& prompt = "Quit?") {
            // 从prompt中提取标题（去掉可能的"?"/"(CTRL+C)"等后缀）
            std::string title = prompt;
            size_t qpos = title.find('?');
            if (qpos != std::string::npos) {
                title = title.substr(0, qpos);
            }
            
            // 边框标题格式: " QUIT PODRADIO "
            std::string border_title = " " + title + " ";
            int border_title_w = Utils::utf8_display_width(border_title);
            
            // 窗口尺寸: 最小宽度要能容纳边框标题
            int min_w = std::max(border_title_w + 4, 24);
            int ih = 4, iw = min_w;  // 高度减少一行（去掉框内标题）
            int iy = h / 2 - ih / 2, ix = (w - iw) / 2;
            
            WINDOW* dlg = newwin(ih, iw, iy, ix);
            keypad(dlg, TRUE);
            
            // 绘制上边框，标题在中间
            // ┌── QUIT PODRADIO ──┐
            waddch(dlg, ACS_ULCORNER);  // ┌
            int side_dashes = (iw - 2 - border_title_w) / 2;
            for (int i = 0; i < side_dashes; i++) waddch(dlg, ACS_HLINE);  // ─
            wprintw(dlg, "%s", border_title.c_str());  // 标题
            int remaining = iw - 2 - side_dashes - border_title_w;
            for (int i = 0; i < remaining; i++) waddch(dlg, ACS_HLINE);  // ─
            waddch(dlg, ACS_URCORNER);  // ┐
            
            // 绘制侧边框和底边框
            mvwaddch(dlg, 1, 0, ACS_VLINE);
            mvwaddch(dlg, 1, iw - 1, ACS_VLINE);
            mvwaddch(dlg, 2, 0, ACS_VLINE);
            mvwaddch(dlg, 2, iw - 1, ACS_VLINE);
            mvwaddch(dlg, 3, 0, ACS_LLCORNER);
            for (int i = 1; i < iw - 1; i++) waddch(dlg, ACS_HLINE);
            waddch(dlg, ACS_LRCORNER);
            
            //YES/NO分布在下部两侧，更美观
            // [Y]es在左边，[N]o在右边
            mvwprintw(dlg, 2, 2, "[Y]es");
            mvwprintw(dlg, 2, iw - 7, "[N]o");
            
            wrefresh(dlg);
            
            // 等待用户输入
            int ch;
            bool result = false;
            while ((ch = wgetch(dlg)) != ERR) {
                if (ch == 'y' || ch == 'Y') {
                    result = true;
                    break;
                } else if (ch == 'n' || ch == 'N' || ch == 27) {  // ESC也取消
                    result = false;
                    break;
                }
            }
            
            delwin(dlg);
            return result;
        }
        
        // V2.39-FF: 公共方法，显示帮助弹窗
        void show_help(const MPVController::State& state) {
            draw_help(nullptr, state, 0);
        }
        
        //切换深色/浅色主题
        void toggle_theme() {
            dark_mode_ = !dark_mode_;
            apply_theme();
            EVENT_LOG(fmt::format("Theme: {}", dark_mode_ ? "Dark" : "Light"));
        }
        
        //获取当前主题模式
        bool is_dark_mode() const { return dark_mode_; }
        
        //应用主题颜色
        void apply_theme() {
            if (dark_mode_) {
                // 深色主题：标准ncurses颜色
                init_pair(10, COLOR_CYAN, COLOR_BLACK);
                init_pair(11, COLOR_GREEN, COLOR_BLACK);
                init_pair(12, COLOR_YELLOW, COLOR_BLACK);
                init_pair(13, COLOR_RED, COLOR_BLACK);
                init_pair(14, COLOR_BLUE, COLOR_BLACK);
                init_pair(15, COLOR_MAGENTA, COLOR_BLACK);
                // 边框颜色
                init_pair(20, COLOR_WHITE, COLOR_BLACK);   // 标准边框
                init_pair(21, COLOR_CYAN, COLOR_BLACK);    // 信息边框
            } else {
                // 浅色主题：反色方案
                init_pair(10, COLOR_CYAN, COLOR_WHITE);
                init_pair(11, COLOR_GREEN, COLOR_WHITE);
                init_pair(12, COLOR_YELLOW, COLOR_WHITE);
                init_pair(13, COLOR_RED, COLOR_WHITE);
                init_pair(14, COLOR_BLUE, COLOR_WHITE);
                init_pair(15, COLOR_MAGENTA, COLOR_WHITE);
                // 边框颜色
                init_pair(20, COLOR_BLACK, COLOR_WHITE);   // 标准边框
                init_pair(21, COLOR_BLUE, COLOR_WHITE);    // 信息边框
            }
            // 刷新所有窗口
            werase(left_win);
            werase(right_win);
            werase(status_win);
            box(left_win, 0, 0);
            box(right_win, 0, 0);
            box(status_win, 0, 0);
            wrefresh(left_win);
            wrefresh(right_win);
            wrefresh(status_win);
        }

    private:
        WINDOW *left_win, *right_win, *status_win;
        int h, w, left_w, right_w, top_h;
        int cols_ = 0;  //追踪之前的列数，用于检测resize
        float layout_ratio_ = 0.4f;
        StatusBarColorConfig statusbar_color_config_;
        bool show_tree_lines_ = true;
        bool scroll_mode_ = false;  //默认关闭滚动显示
        
        //主题模式 - 支持深色/浅色主题
        bool dark_mode_ = true;
        
        //使用统一的布局管理器
        // 所有滚动偏移由LayoutMetrics统一管理，窗口尺寸变化时自动重置
        LayoutMetrics& layout_ = LayoutMetrics::instance();
        
        //增量重绘状态追踪（暂时禁用以确保稳定）
        int last_selected_idx_ = -1;
        size_t last_display_size_ = 0;
        int last_view_start_ = -1;
        AppMode last_mode_ = AppMode::RADIO;  // 追踪模式变化
        
        //检查是否需要完全重绘（暂时总是返回true）
        bool needs_full_redraw(int selected, size_t list_size, int view_start, AppMode mode) {
            (void)selected; (void)list_size; (void)view_start; (void)mode;
            //暂时禁用增量重绘以确保稳定
            return true;
        }
        
        //更新追踪状态
        void update_redraw_state(int selected, size_t list_size, int view_start, AppMode mode) {
            last_selected_idx_ = selected;
            last_display_size_ = list_size;
            last_view_start_ = view_start;
            last_mode_ = mode;
        }
        
        // V0.05B9n3e5g3m: 添加current_url参数用于高亮当前播放节目
        void draw_line(WINDOW* win, int y, const DisplayItem& item, bool selected, bool in_visual, int max_len, const std::string& current_url = "") {
            (void)max_len; // 保留参数供未来使用（内部使用title_max_len）
            // V2.39-FF: 空指针检查
            if (!item.node) return;
            
            //节点树缩进计算详细说明
            // ============================================
            // 节点层级结构示例：
            // depth=0 (根节点): 无prefix, 无connector, 有icon
            // depth=1: prefix=" "(1字符) + connector="├─ "(4字符) = 5字符
            // depth=2: prefix="  "(2字符) + connector="├─ "(4字符) = 6字符
            // depth=N: prefix=N字符 + connector=4字符

            // icon宽度：
            // - ▼/▶ (文件夹): 3字符 (1字符宽度+1空格)
            // - 📺 (视频): 3字符 (2字符宽度+1空格)
            // - 🎵 (电台): 3字符 (2字符宽度+1空格)
            // - 🎙 (播客): 3字符 (2字符宽度+1空格)

            //净空间计算公式
            // 可用净宽 = content_width - fixed_width
            //          = (left_w - 2) - (depth*2 + 4 + icon_width)

            // 其中 content_width = left_w - 2 (完整内容区宽度)
            
            std::string prefix;
            std::string connector;
            
            if (show_tree_lines_ && item.depth > 0) {
                //每个父节点层级占用1个空格缩进
                for (int d = 0; d < item.depth; ++d) {
                    prefix += " ";
                }
                //connector="├─ "或"└─ "，占4个字符位
                connector = item.is_last ? "└─ " : "├─ ";
            } else {
                prefix = std::string(item.depth, ' ');
                connector = "";
            }
            
            std::string icon;
            std::string url = item.node->url.empty() ? "" : item.node->url;
            
            // V0.05B9n3e5g3m: 检测当前播放节目
            // 当前播放优先级最高，使用绿色高亮
            bool is_currently_playing = !current_url.empty() && !url.empty() && url == current_url;
            
            //区分完整缓存（已下载）和部分缓存（流缓存），使用不同颜色
            bool is_downloaded = !url.empty() && 
                                 (CacheManager::instance().is_downloaded(url) || item.node->is_downloaded);
            bool is_stream_cached = !url.empty() && !is_downloaded &&
                                    (CacheManager::instance().is_cached(url) || item.node->is_cached);
            
            //使用IconManager统一管理图标（统一Emoji风格）
            // 所有图标固定占3列（空格+图标+空格），防止宽度不一致
            if (item.node->marked) icon = IconManager::get_marked();
            else if (item.node->parse_failed) icon = IconManager::get_failed();
            else if (item.node->loading) icon = IconManager::get_loading();
            else if (item.node->type == NodeType::FOLDER || item.node->type == NodeType::PODCAST_FEED)
                icon = item.node->expanded ? IconManager::get_folder_expanded() : IconManager::get_folder_collapsed();
            else if (item.node->type == NodeType::RADIO_STREAM) {
                // V2.39-FF: 电台节点根据是否有子节点显示不同图标
                if (!item.node->children.empty() || !item.node->children_loaded) {
                    // 有子节点或尚未加载，显示文件夹图标
                    icon = item.node->expanded ? IconManager::get_folder_expanded() : IconManager::get_folder_collapsed();
                } else {
                    // 末级电台节点，显示音乐图标
                    icon = IconManager::get_radio();
                }
            }
            else if (item.node->type == NodeType::PODCAST_EPISODE) {
                // V2.39: 检测视频节目
                if (!url.empty()) {
                    URLType url_type = URLClassifier::classify(url);
                    if (item.node->is_youtube || url_type == URLType::VIDEO_FILE) icon = IconManager::get_video();
                    else icon = IconManager::get_podcast();
                } else {
                    icon = IconManager::get_podcast();
                }
            }
            
            //图标区域固定宽度
            // 无论ASCII还是Emoji，图标区域固定占ICON_FIELD_WIDTH列
            int icon_width = IconManager::get_icon_field_width();
            
            //固定部分（prefix + connector + icon），只有标题滚动
            //图标宽度使用固定值，消除不确定性
            std::string fixed_part = prefix + connector + icon;
            int fixed_width = Utils::utf8_display_width(prefix + connector) + icon_width;
            
            //充分利用内容区宽度
            //使用安全内容区宽度，防止Emoji溢出
            int safe_content_width = layout_.get_metrics().safe_content_w;
            int title_max_len = safe_content_width - fixed_width;
            if (title_max_len < 1) title_max_len = 1;  // 至少保留1个字符
            
            std::string title_display;
            int title_width = Utils::utf8_display_width(item.node->title);
            if (scroll_mode_ && title_width > title_max_len) {
                //滚动模式开启，使用工业级滚动引擎
                //使用LayoutMetrics统一管理滚动偏移
                layout_.increment_line_scroll_offset(y);
                int scroll_offset = layout_.get_line_scroll_offset(y);
                title_display = Utils::get_scrolling_text(item.node->title, title_max_len, scroll_offset / 5);
            } else {
                //滚动模式关闭，使用严格截断显示
                title_display = Utils::truncate_by_display_width_strict(item.node->title, title_max_len);
            }
            
            //双重保护 - 确保最终显示严格不超出安全内容区宽度
            std::string final_display = fixed_part + title_display;
            int final_width = Utils::utf8_display_width(final_display);
            if (final_width > safe_content_width) {
                final_display = Utils::truncate_by_display_width_strict(final_display, safe_content_width);
            }
            
            // V0.05B9n3e5g3m: 颜色优先级调整
            // 1. 选中/Visual模式 - 反白显示（最高优先级）
            // 2. 当前播放 - 绿色（与播放列表指示一致）
            // 3. 完整下载 - 洋红色（青黄色系）
            // 4. 流缓存 - 青色
            // 5. 数据库缓存 - 黄色
            // 6. 解析失败 - 红色
            if (selected || in_visual) wattron(win, A_REVERSE);
            else if (is_currently_playing) wattron(win, COLOR_PAIR(11));    //绿色 - 当前播放
            else if (is_downloaded) wattron(win, COLOR_PAIR(15));           //洋红色 - 完整下载
            else if (is_stream_cached) wattron(win, COLOR_PAIR(10));        //青色 - 流缓存
            else if (item.node->is_db_cached) wattron(win, COLOR_PAIR(12)); //黄色 - 数据库缓存
            else if (item.node->parse_failed) wattron(win, COLOR_PAIR(13)); //红色 - 解析失败
            
            //打印逻辑
            // 窗口结构：列0=左边框，列1到列left_w-2=内容区，列left_w-1=右边框
            //使用安全内容区宽度，防止Emoji溢出
            
            // 步骤1：移动到行首（列1，内容区开始位置）
            wmove(win, y, 1);
            
            // 步骤2：用空格清除整行内容区（防止残留）
            std::string clear_str(safe_content_width, ' ');
            waddstr(win, clear_str.c_str());
            
            // 步骤3：移动回行首，打印实际内容
            wmove(win, y, 1);
            //直接使用waddstr，因为final_display已经严格截断
            // waddnstr的第二个参数是字节数不是显示宽度，对UTF-8字符串会造成问题
            waddstr(win, final_display.c_str());
            
            if (selected || in_visual) wattroff(win, A_REVERSE);
            else if (is_currently_playing) wattroff(win, COLOR_PAIR(11));
            else if (is_downloaded) wattroff(win, COLOR_PAIR(15));
            else if (is_stream_cached) wattroff(win, COLOR_PAIR(10));
            else if (item.node->is_db_cached) wattroff(win, COLOR_PAIR(12));
            else if (item.node->parse_failed) wattroff(win, COLOR_PAIR(13));
        }

        //响应式状态栏 - 智能缩略策略
        // ┌─────────────────────────────────────────────────────────────────────────┐
        // │ 【状态栏结构】                                                            │
        // │   ░▒▓█ ★ PODRADIO V0.05B9n3e5g2RF ★ █▓▒░   [ URL ]   ░▒▓█ By 作者时间 █▓▒░ │
        // │   └────────── 左侧艺术效果 ──────────┘   └中间┘   └────────── 右侧艺术效果 ─┘ │
        // ├─────────────────────────────────────────────────────────────────────────┤
        // │ 【保护规则】                                                              │
        // │   左侧固定：░▒▓█ ★  和  █▓▒░（外侧艺术效果始终保留）                        │
        // │   右侧固定：░▒▓█ By 和  █▓▒░（外侧艺术效果始终保留）                        │
        // │   中间固定：[ 和 ]（方括号本身始终保留）                                    │
        // ├─────────────────────────────────────────────────────────────────────────┤
        // │ 【缩略优先级（从高到低）】                                                  │
        // │   优先级1：中间 [] 内的URL内容 - 从中间用...截断                           │
        // │   优先级2：左侧版本号 - 从右侧（靠近中间方向）截断                           │
        // │   优先级3：右侧作者时间 - 从左侧（靠近中间方向）截断                         │
        // └─────────────────────────────────────────────────────────────────────────┘
        void draw_status(WINDOW* win, const MPVController::State& state, TreeNodePtr selected_node) {
            werase(win);
            box(win, 0, 0);
            int ww = getmaxx(win);
            int inner_w = ww - 2;  // 可用宽度（减去左右边框）
            
            // ═══════════════════════════════════════════════════════════════════
            //使用模块化艺术效果配置
            // ═══════════════════════════════════════════════════════════════════
            
            // 可变内容
            std::string version_str = fmt::format("{} {}", APP_NAME, VERSION);
            std::string author_time = fmt::format("{}@{}", AUTHOR, BUILD_TIME);
            
            // 获取中间URL内容（方括号内的部分）
            std::string mid_content;  // 方括号内的内容
            bool show_timer = SleepTimer::instance().is_active();
            if (show_timer) {
                int remaining = SleepTimer::instance().remaining_seconds();
                int hours = remaining / 3600;
                int minutes = (remaining % 3600) / 60;
                int seconds = remaining % 60;
                mid_content = fmt::format("⏰ {:02d}:{:02d}:{:02d}", hours, minutes, seconds);
            } else {
                std::string url_to_show = state.has_media ? state.current_url : (selected_node ? selected_node->url : "");
                url_to_show = Utils::http_to_https(url_to_show);
                mid_content = url_to_show;
            }
            
            // ═══════════════════════════════════════════════════════════════════
            // 计算各部分宽度
            // ═══════════════════════════════════════════════════════════════════
            
            //使用全局模块化艺术效果配置
            // 左侧结构: ART_OUTER_LEFT + 版本号 + ART_OUTER_RIGHT
            // 右侧结构: ART_RIGHT_PREFIX + 作者时间 + ART_RIGHT_SUFFIX
            // 中间结构: ART_BRACKET_LEFT + 内容 + ART_BRACKET_RIGHT
            
            // 固定艺术字符宽度
            int left_art_w = Utils::utf8_display_width(ART_OUTER_LEFT) + Utils::utf8_display_width(ART_OUTER_RIGHT);
            int right_art_w = Utils::utf8_display_width(ART_RIGHT_PREFIX) + Utils::utf8_display_width(ART_RIGHT_SUFFIX);
            
            // 可变内容宽度
            int version_w = Utils::utf8_display_width(version_str);
            int author_w = Utils::utf8_display_width(author_time);
            int mid_w = Utils::utf8_display_width(mid_content);
            
            // 中间[]固定部分
            int bracket_fixed_w = Utils::utf8_display_width(ART_BRACKET_LEFT) + Utils::utf8_display_width(ART_BRACKET_RIGHT);
            
            // 完整显示所需总宽度
            int total_needed = left_art_w + version_w + right_art_w + author_w + bracket_fixed_w + mid_w;
            
            // 颜色
            int left_color = StatusBarColorRenderer::get_color(statusbar_color_config_, 0);
            int right_color = StatusBarColorRenderer::get_color(statusbar_color_config_, 180);
            int mid_color = StatusBarColorRenderer::get_color(statusbar_color_config_, 90);
            
            // ═══════════════════════════════════════════════════════════════════
            //智能缩略逻辑（使用模块化艺术配置）
            // ═══════════════════════════════════════════════════════════════════
            
            std::string left_display, mid_display, right_display;
            
            // 计算固定部分总宽度（艺术字符 + []固定部分）
            int fixed_total = left_art_w + right_art_w + bracket_fixed_w;
            
            if (total_needed <= inner_w) {
                // ════ 情况1：宽度足够，完整显示 ════
                left_display = std::string(ART_OUTER_LEFT) + version_str + ART_OUTER_RIGHT;
                mid_display = mid_content.empty() ? "" : std::string(ART_BRACKET_LEFT) + mid_content + ART_BRACKET_RIGHT;
                right_display = std::string(ART_RIGHT_PREFIX) + author_time + ART_RIGHT_SUFFIX;
                
            } else if (inner_w < fixed_total + 6) {
                // ════ 情况2：极端窄窗口，保留艺术字符，内容缩略为... ════
                left_display = std::string(ART_OUTER_LEFT) + "..." + ART_OUTER_RIGHT;
                mid_display = std::string(ART_BRACKET_LEFT) + "..." + ART_BRACKET_RIGHT;
                right_display = std::string(ART_RIGHT_PREFIX) + "..." + ART_RIGHT_SUFFIX;
                
            } else {
                // ════ 情况3：需要按优先级缩略 ════
                // 可变区域可用宽度
                int variable_available = inner_w - fixed_total;
                
                // 阶段1：先尝试完整显示版本号和作者时间，缩略中间URL
                int lr_needed = version_w + author_w;
                int mid_available = variable_available - lr_needed;
                
                if (mid_available >= 3) {
                    // 版本号和作者时间可以完整显示，缩略中间URL
                    if (mid_w <= mid_available) {
                        mid_display = std::string(ART_BRACKET_LEFT) + mid_content + ART_BRACKET_RIGHT;
                    } else {
                        // 从中间截断URL
                        int inner_bracket_w = mid_available - bracket_fixed_w;
                        mid_display = std::string(ART_BRACKET_LEFT) + Utils::truncate_middle(mid_content, inner_bracket_w) + ART_BRACKET_RIGHT;
                    }
                    left_display = std::string(ART_OUTER_LEFT) + version_str + ART_OUTER_RIGHT;
                    right_display = std::string(ART_RIGHT_PREFIX) + author_time + ART_RIGHT_SUFFIX;
                    
                } else {
                    // 阶段2：需要缩略版本号和/或作者时间
                    // 先给中间URL最小空间（3字符 "..."）
                    int mid_min = 3;
                    int remaining_for_lr = variable_available - mid_min;
                    
                    if (remaining_for_lr < 4) {
                        // 空间太小，只显示艺术字符
                        left_display = ART_OUTER_LEFT;
                        mid_display = std::string(ART_BRACKET_LEFT) + "..." + ART_BRACKET_RIGHT;
                        right_display = ART_RIGHT_SUFFIX;
                    } else {
                        // 分配剩余空间给版本号和作者时间
                        int half_remaining = remaining_for_lr / 2;
                        
                        // 缩略版本号
                        std::string left_version_part;
                        if (version_w <= half_remaining) {
                            left_version_part = version_str;
                        } else if (half_remaining > 3) {
                            left_version_part = Utils::truncate_by_display_width(version_str, half_remaining - 3) + "...";
                        } else {
                            left_version_part = "...";
                        }
                        left_display = std::string(ART_OUTER_LEFT) + left_version_part + ART_OUTER_RIGHT;
                        
                        // 缩略作者时间
                        int right_remaining = remaining_for_lr - Utils::utf8_display_width(left_version_part);
                        std::string right_author_part;
                        if (author_w <= right_remaining) {
                            right_author_part = author_time;
                        } else if (right_remaining > 3) {
                            right_author_part = "..." + Utils::truncate_by_display_width_right(author_time, right_remaining - 3);
                        } else {
                            right_author_part = "...";
                        }
                        right_display = std::string(ART_RIGHT_PREFIX) + right_author_part + ART_RIGHT_SUFFIX;
                        
                        // 中间只显示 [...]
                        mid_display = std::string(ART_BRACKET_LEFT) + "..." + ART_BRACKET_RIGHT;
                    }
                }
            }
            
            // ═══════════════════════════════════════════════════════════════════
            //[] 居中显示逻辑 - 优先缩略中间URL保护艺术字符
            // ═══════════════════════════════════════════════════════════════════
            
            // 计算中间区域居中位置
            int mid_display_w = Utils::utf8_display_width(mid_display);
            int mid_x = (ww - mid_display_w) / 2;  // 居中起始位置
            if (mid_x < 1) mid_x = 1;  // 保护左边框
            
            // 计算左侧最大可用宽度：到中间区域左侧边界
            int left_max_w = mid_x - 2;  // 减去左边框和空格
            if (left_max_w < 0) left_max_w = 0;
            
            // 计算右侧最大可用宽度：从中间区域右侧到右边框
            int right_start_x = mid_x + mid_display_w + 1;  // 中间内容结束后位置
            int right_max_w = ww - right_start_x - 1;  // 减去右边框
            if (right_max_w < 0) right_max_w = 0;
            
            //优先缩略中间URL，保护左右艺术字符完整性
            int left_display_w = Utils::utf8_display_width(left_display);
            int right_display_w = Utils::utf8_display_width(right_display);
            
            // 检查是否需要缩略中间URL
            bool need_shrink_mid = false;
            int total_overflow = 0;
            
            if (left_display_w > left_max_w) {
                total_overflow += (left_display_w - left_max_w);
                need_shrink_mid = true;
            }
            if (right_display_w > right_max_w) {
                total_overflow += (right_display_w - right_max_w);
                need_shrink_mid = true;
            }
            
            // 如果需要缩略中间URL
            if (need_shrink_mid && mid_display_w > 8) {  // 至少保留 "[ ... ]"
                int new_mid_w = mid_display_w - total_overflow - 4;  // 额外减少4字符确保对齐
                if (new_mid_w < 8) new_mid_w = 8;
                
                // 重新构建缩略后的中间内容
                if (!mid_content.empty()) {
                    int inner_w = new_mid_w - 4;  // "[ " 和 " ]" 占4字符
                    if (inner_w < 3) inner_w = 3;
                    mid_display = "[ " + Utils::truncate_middle(mid_content, inner_w) + " ]";
                } else {
                    mid_display = "[...]";
                }
                
                // 重新计算居中位置
                mid_display_w = Utils::utf8_display_width(mid_display);
                mid_x = (ww - mid_display_w) / 2;
                if (mid_x < 1) mid_x = 1;
                
                // 重新计算左右可用宽度
                left_max_w = mid_x - 2;
                if (left_max_w < 0) left_max_w = 0;
                
                right_start_x = mid_x + mid_display_w + 1;
                right_max_w = ww - right_start_x - 1;
                if (right_max_w < 0) right_max_w = 0;
            }
            
            // 最后保险：如果仍然超出，截断版本号（保护艺术字符后缀）
            left_display_w = Utils::utf8_display_width(left_display);
            if (left_display_w > left_max_w && left_max_w > 0) {
                // 保护后缀艺术字符，只截断版本号
                int suffix_w = Utils::utf8_display_width(ART_OUTER_RIGHT);
                int prefix_w = Utils::utf8_display_width(ART_OUTER_LEFT);
                int available_for_version = left_max_w - prefix_w - suffix_w;
                if (available_for_version > 0) {
                    left_display = std::string(ART_OUTER_LEFT) + 
                        Utils::truncate_by_display_width(version_str, available_for_version) + 
                        ART_OUTER_RIGHT;
                } else {
                    left_display = std::string(ART_OUTER_LEFT) + ART_OUTER_RIGHT;
                    if (Utils::utf8_display_width(left_display) > left_max_w) {
                        left_display = Utils::truncate_by_display_width(left_display, left_max_w);
                    }
                }
            }
            
            // 最后保险：如果右侧仍然超出，截断作者时间（保护艺术字符后缀）
            right_display_w = Utils::utf8_display_width(right_display);
            if (right_display_w > right_max_w && right_max_w > 0) {
                // 保护后缀艺术字符，只截断作者时间
                int suffix_w = Utils::utf8_display_width(ART_RIGHT_SUFFIX);
                int prefix_w = Utils::utf8_display_width(ART_RIGHT_PREFIX);
                int available_for_author = right_max_w - prefix_w - suffix_w;
                if (available_for_author > 0) {
                    right_display = std::string(ART_RIGHT_PREFIX) + 
                        Utils::truncate_by_display_width_right(author_time, available_for_author) + 
                        ART_RIGHT_SUFFIX;
                } else {
                    right_display = std::string(ART_RIGHT_PREFIX) + ART_RIGHT_SUFFIX;
                    if (Utils::utf8_display_width(right_display) > right_max_w) {
                        right_display = Utils::truncate_by_display_width_right(right_display, right_max_w);
                    }
                }
            }
            
            // ═══════════════════════════════════════════════════════════════════
            // 绘制状态栏（[]居中）
            // ═══════════════════════════════════════════════════════════════════
            
            // 左侧（从x=1开始，不超过中间区域左侧）
            wattron(win, COLOR_PAIR(left_color + 1));
            mvwprintw(win, 1, 1, "%s", left_display.c_str());
            wattroff(win, COLOR_PAIR(left_color + 1));
            
            // 中间（固定居中）
            if (!mid_display.empty()) {
                wattron(win, COLOR_PAIR(mid_color + 1));
                mvwprintw(win, 1, mid_x, "%s", mid_display.c_str());
                wattroff(win, COLOR_PAIR(mid_color + 1));
            }
            
            // 右侧（右对齐，不超过中间区域右侧）
            right_display_w = Utils::utf8_display_width(right_display);
            int right_x = ww - right_display_w - 1;
            // 确保右侧不与中间重叠
            if (right_x < right_start_x) {
                // 空间冲突，需要缩略
                int new_right_w = ww - right_start_x - 1;
                if (new_right_w > 0) {
                    //使用模块化艺术配置常量
                    int suffix_w = Utils::utf8_display_width(ART_RIGHT_SUFFIX);
                    int prefix_w = Utils::utf8_display_width(ART_RIGHT_PREFIX);
                    int available_for_author = new_right_w - prefix_w - suffix_w;
                    if (available_for_author > 0) {
                        right_display = std::string(ART_RIGHT_PREFIX) + 
                            Utils::truncate_by_display_width_right(author_time, available_for_author) + 
                            ART_RIGHT_SUFFIX;
                    } else if (new_right_w >= prefix_w + suffix_w) {
                        right_display = std::string(ART_RIGHT_PREFIX) + ART_RIGHT_SUFFIX;
                    } else {
                        right_display = ART_RIGHT_SUFFIX;
                        if (Utils::utf8_display_width(right_display) > new_right_w) {
                            right_display = "";
                        }
                    }
                    right_x = right_start_x;
                } else {
                    right_display = "";
                    right_x = right_start_x;
                }
            }
            if (!right_display.empty()) {
                wattron(win, COLOR_PAIR(right_color + 1));
                mvwprintw(win, 1, right_x, "%s", right_display.c_str());
                wattroff(win, COLOR_PAIR(right_color + 1));
            }
            
            //状态栏边框保护（无标题嵌入）
            protect_border(win, ww, 3);
            
            //移除wrefresh，由draw()统一双缓冲刷新
        }

        // V2.39: INFO区域显示
        //新增播放列表参数
        //新增List模式参数
        void draw_info(WINDOW* win, const MPVController::State& state, AppState app_state,
                       TreeNodePtr playback_node, int marked_count, const std::string& search_query,
                       int current_match, int total_matches, TreeNodePtr selected_node,
                       const std::vector<DownloadProgress>& downloads, bool visual_mode, int cw,
                       const std::vector<PlaylistItem>& playlist = {}, int playlist_index = -1,
                       const std::vector<SavedPlaylistItem>& saved_playlist = {}, int list_selected_idx = 0) {
            //截断宽度计算
            // 右面板结构（窗口宽度 right_w）：
            //   列0 = 左边框 │
            //   列1 = 左边距（空格）
            //   列2 ~ 列right_w-3 = 内容区
            //   列right_w-2 = 右边距（空格）
            //   列right_w-1 = 右边框 │

            // 传入参数：cw = right_w - 3（内容区宽度+右边距）
            // 从列2开始打印，最后一列可用是right_w-3
            // 可用宽度 = (right_w-3) - 2 + 1 = right_w - 4 = cw - 1

            // 实际内容区到列right_w-3，右边距在列right_w-2
            // 所以截断宽度 = cw（内容区+右边距，充分利用）
            int safe_cw = cw;
            if (safe_cw < 1) safe_cw = 1;  // 最小宽度保护
            
            int border_bottom = top_h - 1;
            int log_height = std::max(6, static_cast<int>((top_h - 2) * 0.4));
            int split_y = border_bottom - log_height;
            
            int y = 1;
            
            if (visual_mode) {
                wattron(win, A_BOLD);
                mvwprintw(win, y++, 2, "-- VISUAL MODE --");
                wattroff(win, A_BOLD);
                mvwprintw(win, y++, 2, "j/k: extend | v: confirm | Esc: cancel | V: clear all");
                y++;
            }
            
            if (!search_query.empty()) {
                wattron(win, A_BOLD);
                std::string search_info = fmt::format("Search: \"{}\" ({}/{})", search_query, 
                                                      total_matches > 0 ? current_match + 1 : 0, total_matches);
                mvwprintw(win, y++, 2, "%s", Utils::truncate_by_display_width(search_info, safe_cw).c_str());
                wattroff(win, A_BOLD);
                y++;
            }
            
            // V0.03: 改进的播放状态显示，包含音量和速度
            std::string state_str;
            std::string state_icon;
            switch (app_state) {
                case AppState::LOADING: state_icon = "⏳"; state_str = "Loading..."; break;
                case AppState::PLAYING: state_icon = "▶"; state_str = "Playing"; break;
                case AppState::PAUSED: state_icon = "⏸"; state_str = "Paused"; break;
                case AppState::BUFFERING: state_icon = "⏳"; state_str = "Buffering..."; break;
                case AppState::BROWSING: state_icon = "🎯"; state_str = "Navigating"; break;  //靶心图标表示导航定位
                case AppState::LIST_MODE: state_icon = "📋"; state_str = "List Mode"; break;  //List模式
                default: state_icon = "●"; state_str = "Idle"; break;
            }
            
            //播放/暂停时不显示此状态行（PLAYER STATUS区域已显示相同信息）
            if (app_state != AppState::PLAYING && app_state != AppState::PAUSED) {
                // V0.03: 状态行显示: 状态 | 音量 | 速度
                //精确计算截断宽度

                //精确计算截断宽度
                // 右面板结构（窗口宽度 right_w）：
                //   列0 = 左边框 │
                //   列1 = 左边距（空格）
                //   列2 ~ 列right_w-3 = 内容区
                //   列right_w-2 = 右边距（空格）
                //   列right_w-1 = 右边框 │

                // 传入参数：cw = right_w - 3（内容区宽度）
                // 从列2开始打印，可用宽度 = cw

                // 截断宽度 = safe_cw（安全保护右边框）
                std::string status_line = fmt::format("{} {} | Vol:{}% | Spd:{:.1f}x", 
                                                       state_icon, state_str, state.volume, state.speed);
                wattron(win, A_BOLD);
                mvwprintw(win, y++, 2, "%s", Utils::truncate_by_display_width(status_line, safe_cw).c_str());
                wattroff(win, A_BOLD);
            }
            
            if (marked_count > 0) {
                mvwprintw(win, y++, 2, "Marked: %d items", marked_count);
                mvwprintw(win, y++, 2, "Enter: Play | d: Del | D: DL");
                y++;
            }

            //List模式下显示选中项的详细信息
            if (app_state == AppState::LIST_MODE && !saved_playlist.empty() &&
                list_selected_idx >= 0 && list_selected_idx < static_cast<int>(saved_playlist.size())) {
                y++;
                wattron(win, A_BOLD);
                mvwprintw(win, y++, 2, "--- Selected Item ---");
                wattroff(win, A_BOLD);

                const auto& item = saved_playlist[list_selected_idx];
                mvwprintw(win, y++, 2, "Title: %s",
                          Utils::truncate_by_display_width(item.title, safe_cw - 8).c_str());
                mvwprintw(win, y++, 2, "URL:");
                std::string url_display = "  " + item.url;
                mvwprintw(win, y++, 2, "%s",
                          Utils::truncate_by_display_width(url_display, safe_cw).c_str());

                if (item.duration > 0) {
                    int mins = item.duration / 60;
                    int secs = item.duration % 60;
                    mvwprintw(win, y++, 2, "Duration: %d:%02d", mins, secs);
                }
                mvwprintw(win, y++, 2, "Type: %s", item.is_video ? "Video" : "Audio");
                y++;

                // 显示播放列表统计
                mvwprintw(win, y++, 2, "Total items: %zu", saved_playlist.size());
                mvwprintw(win, y++, 2, "Selected: %d/%zu", list_selected_idx + 1, saved_playlist.size());
                y++;
            }
            
            if (!downloads.empty()) {
                y++;
                wattron(win, A_BOLD);
                mvwprintw(win, y++, 2, "--- Downloads ---");
                wattroff(win, A_BOLD);
                
                for (const auto& dl : downloads) {
                    if (y >= split_y - 3) break;
                    
                    //增强下载状态显示
                    std::string status_line = dl.title;
                    if (dl.active) {
                        status_line += fmt::format(" [{}%]", dl.percent);
                    } else if (dl.complete) {
                        status_line += " [OK]";
                    } else if (dl.failed) {
                        status_line += " [FAIL]";
                    }
                    
                    mvwprintw(win, y++, 2, "%s", Utils::truncate_by_display_width(status_line, safe_cw).c_str());
                    
                    //ASCII艺术进度条 + 速率 + ETA（宽度自适应）
                    if (dl.active && y < split_y - 2 && dl.percent > 0) {
                        //进度条宽度自适应窗口（留出速率和ETA空间）
                        int bar_width = std::max(MIN_PROGRESS_BAR_WIDTH, safe_cw - PROGRESS_BAR_RESERVED_SPACE);
                        if (bar_width > MAX_PROGRESS_BAR_WIDTH) bar_width = MAX_PROGRESS_BAR_WIDTH;
                        int filled = (dl.percent * bar_width) / 100;
                        
                        // ASCII艺术进度条: [████████░░░░░]
                        std::string bar = "[";
                        for (int i = 0; i < bar_width; ++i) {
                            bar += (i < filled) ? "█" : "░";
                        }
                        bar += "]";
                        
                        //速率和ETA信息（始终显示）
                        std::string speed_eta;
                        if (!dl.speed.empty()) {
                            speed_eta = dl.speed;
                        } else {
                            speed_eta = "...";  // 无速率时显示占位符
                        }
                        if (dl.eta_seconds > 0) {
                            int eta_mins = dl.eta_seconds / 60;
                            int eta_secs = dl.eta_seconds % 60;
                            speed_eta += fmt::format(" ETA:{}:{:02d}", eta_mins, eta_secs);
                        } else if (dl.percent < 100) {
                            speed_eta += " ETA:--:--";  // 无ETA时显示占位符
                        }
                        
                        wattron(win, COLOR_PAIR(11));
                        mvwprintw(win, y++, 2, "%s %s", bar.c_str(), speed_eta.c_str());
                        wattroff(win, COLOR_PAIR(11));
                    }
                }
                y++;
            }
            
            if (app_state == AppState::PLAYING || app_state == AppState::PAUSED) {
                // V2.39-FF: 添加播放器状态标题
                y++;
                wattron(win, A_BOLD);
                mvwprintw(win, y++, 2, "=== PLAYER STATUS ===");
                wattroff(win, A_BOLD);
                
                // V2.39-FF: 显示播放状态和速度/音量
                std::string play_state = (app_state == AppState::PLAYING) ? "▶ Playing" : "⏸ Paused";
                mvwprintw(win, y++, 2, "%s", play_state.c_str());
                mvwprintw(win, y++, 2, "Speed:  %.1fx", state.speed);
                mvwprintw(win, y++, 2, "Volume: %d%%", state.volume);
                
                //ASCII艺术时间轴进度条（宽度自适应）
                if (state.media_duration > 0) {
                    int cur_mins = (int)state.time_pos / 60;
                    int cur_secs = (int)state.time_pos % 60;
                    int tot_mins = (int)state.media_duration / 60;
                    int tot_secs = (int)state.media_duration % 60;
                    
                    // 计算进度百分比
                    double progress = state.time_pos / state.media_duration;
                    if (progress > 1.0) progress = 1.0;
                    if (progress < 0.0) progress = 0.0;
                    
                    //时间轴宽度自适应窗口
                    int time_str_len = 16;  // 时间字符串 " 3:45 / 12:30" 约16字符
                    int bar_width = cw - time_str_len - 4;  // 留出时间和边距空间
                    if (bar_width < MIN_PROGRESS_BAR_WIDTH) bar_width = MIN_PROGRESS_BAR_WIDTH;
                    if (bar_width > 40) bar_width = 40;     // 时间轴最大40字符
                    int filled = static_cast<int>(progress * bar_width);
                    
                    std::string timeline = "[";
                    for (int i = 0; i < bar_width; ++i) {
                        if (i < filled) {
                            timeline += "▓";  // 已播放部分
                        } else if (i == filled) {
                            timeline += (app_state == AppState::PLAYING) ? "▶" : "⏸";  // 播放头
                        } else {
                            timeline += "░";  // 未播放部分
                        }
                    }
                    timeline += "]";
                    
                    // 时间显示
                    std::string time_str = fmt::format(" {}:{:02d}/{}:{:02d}", cur_mins, cur_secs, tot_mins, tot_secs);
                    
                    wattron(win, A_BOLD);
                    mvwprintw(win, y++, 2, "%s%s", timeline.c_str(), time_str.c_str());
                    wattroff(win, A_BOLD);
                } else if (state.time_pos > 0) {
                    // 无总时长时只显示当前时间
                    int cur_mins = (int)state.time_pos / 60;
                    int cur_secs = (int)state.time_pos % 60;
                    mvwprintw(win, y++, 2, "Time:   %d:%02d", cur_mins, cur_secs);
                }
                
                //Video/Audio信息截断处理，防止溢出
                if (!state.audio_codec.empty()) {
                    mvwprintw(win, y++, 2, "Audio:  %s", 
                              Utils::truncate_by_display_width(state.audio_codec, safe_cw - 9).c_str());
                }
                if (!state.video_codec.empty()) {
                    mvwprintw(win, y++, 2, "Video:  %s", 
                              Utils::truncate_by_display_width(state.video_codec, safe_cw - 9).c_str());
                }
                y++;
                
                std::string title_display = (playback_node && !playback_node->title.empty()) ? 
                                            playback_node->title : state.title;
                if (!title_display.empty()) {
                    mvwprintw(win, y++, 2, "Title: %s", 
                              Utils::truncate_by_display_width(title_display, safe_cw - 7).c_str());
                }
            }
            
            if (selected_node && y < split_y) {
                y++;
                std::string type_str;
                switch (selected_node->type) {
                    case NodeType::FOLDER: type_str = "Folder"; break;
                    case NodeType::RADIO_STREAM: type_str = "Radio"; break;
                    case NodeType::PODCAST_FEED: type_str = "Feed"; break;
                    case NodeType::PODCAST_EPISODE: type_str = "Episode"; break;
                }
                
                URLType url_type = URLClassifier::classify(selected_node->url);
                
                mvwprintw(win, y++, 2, "Type: %s", type_str.c_str());
                mvwprintw(win, y++, 2, "Title: %s", 
                          Utils::truncate_by_display_width(selected_node->title, safe_cw - 7).c_str());
                
                if (!selected_node->url.empty()) {
                    //HTTP转HTTPS显示 - 安全优先
                    std::string url = Utils::http_to_https(selected_node->url);
                    
                    //URL显示格式优化 - 标签独占一行，URL换行缩进2空格
                    // 格式：
                    //   URL:
                    //     https://...
                    int url_width = safe_cw - 4;  // 缩进2空格 + 2空格边距
                    if (url_width < 10) url_width = 10;
                    
                    // 标签独占一行
                    mvwprintw(win, y++, 2, "URL:");
                    
                    // URL换行显示，缩进2空格
                    std::vector<std::string> url_lines = Utils::wrap_text(url, url_width, 12);
                    for (size_t li = 0; li < url_lines.size() && y < split_y - 1; ++li) {
                        mvwprintw(win, y++, 2, "  %s", url_lines[li].c_str());
                    }
                    
                    //显示Streaming URL（当与原始URL不同时）
                    //格式优化 - 标签独占一行
                    if (state.has_media && !state.current_url.empty()) {
                        std::string streaming_url = Utils::http_to_https(state.current_url);
                        // 只有当Streaming URL与原始URL不同时才显示
                        if (streaming_url != url) {
                            // 标签独占一行
                            if (y < split_y - 1) {
                                mvwprintw(win, y++, 2, "Streaming URL:");
                            }
                            
                            // URL换行显示，缩进2空格
                            int stream_url_width = safe_cw - 4;
                            if (stream_url_width < 10) stream_url_width = 10;
                            
                            std::vector<std::string> stream_lines = Utils::wrap_text(streaming_url, stream_url_width, 12);
                            for (size_t li = 0; li < stream_lines.size() && y < split_y - 1; ++li) {
                                mvwprintw(win, y++, 2, "  %s", stream_lines[li].c_str());
                            }
                        }
                    }
                }
                
                //修复subtext显示 - 区分节点类型，始终截断
                if (!selected_node->subtext.empty()) {
                    // PODCAST_FEED类型显示"Podcast:"，其他类型显示"Date:"
                    std::string label = (selected_node->type == NodeType::PODCAST_FEED) ? "Podcast:" : "Date:";
                    // 标签占8-9个字符，从第2列开始，需要截断
                    int subtext_max_width = safe_cw - 12;
                    if (subtext_max_width < 10) subtext_max_width = 10;
                    mvwprintw(win, y++, 2, "%s %s", label.c_str(),
                              Utils::truncate_by_display_width(selected_node->subtext, subtext_max_width).c_str());
                }
                if (selected_node->duration > 0) {
                    mvwprintw(win, y++, 2, "Dur:   %s", Utils::format_duration(selected_node->duration).c_str());
                }
                
                if (selected_node->is_cached || CacheManager::instance().is_cached(selected_node->url)) {
                    wattron(win, COLOR_PAIR(11));
                    mvwprintw(win, y++, 2, " [CACHED]");
                    wattroff(win, COLOR_PAIR(11));
                }
                if (selected_node->is_downloaded || CacheManager::instance().is_downloaded(selected_node->url)) {
                    wattron(win, COLOR_PAIR(11));
                    std::string local = CacheManager::instance().get_local_file(selected_node->url);
                    if (!local.empty()) {
                        mvwprintw(win, y++, 2, " [DOWNLOADED: %s]", 
                                  Utils::truncate_by_display_width(local, safe_cw - 16).c_str());
                    } else {
                        mvwprintw(win, y++, 2, " [DOWNLOADED]");
                    }
                    wattroff(win, COLOR_PAIR(11));
                }
                
                if (URLClassifier::is_youtube(url_type)) {
                    wattron(win, COLOR_PAIR(12));
                    mvwprintw(win, y++, 2, " [YouTube]");
                    wattroff(win, COLOR_PAIR(12));
                } else if (url_type == URLType::VIDEO_FILE) {
                    wattron(win, COLOR_PAIR(12));
                    mvwprintw(win, y++, 2, " [Video]");
                    wattroff(win, COLOR_PAIR(12));
                }
            }
            
            // V0.05B9n3e5g3n: 显示播放列表 - 统一L模式和默认模式
            // 优先显示saved_playlist（L模式），否则显示playlist（默认模式）
            // L模式：saved_playlist不为空，使用完整列表
            // 默认模式：saved_playlist为空，使用临时收集的兄弟节点列表
            // 
            // 显示优化：
            // - 显示5行节目（当前项 + 前2个 + 后2个）
            // - 上下缩略说明："↑ N more above" / "↓ N more below"
            // - 以当前播放项为中心定位
            // - EMOJI对齐：🔊 + 1空格 = 3列，普通项 = 3空格
            bool has_saved_playlist = !saved_playlist.empty();
            bool has_playlist = !playlist.empty();
            
            if ((has_saved_playlist || has_playlist) && y < split_y - 3) {
                y++;
                wattron(win, A_BOLD);
                size_t list_size = has_saved_playlist ? saved_playlist.size() : playlist.size();
                mvwprintw(win, y++, 2, "--- Playlist (%zu items) ---", list_size);
                wattroff(win, A_BOLD);
                
                // V0.05B9n3e5g3n: 显示5行节目（当前项 + 前2个 + 后2个）
                const int max_show = 5;
                // 使用list_selected_idx作为中心定位（L模式）或playlist_index（默认模式）
                int center_idx = has_saved_playlist ? list_selected_idx : playlist_index;
                
                // 计算显示范围：以当前播放项为中心，前后各2个
                int start_idx = center_idx - 2;
                int end_idx = center_idx + 3;  // center + 2 + 1(不含end)
                
                // 边界调整
                if (start_idx < 0) {
                    start_idx = 0;
                    end_idx = std::min((int)list_size, max_show);
                }
                if (end_idx > (int)list_size) {
                    end_idx = (int)list_size;
                    start_idx = std::max(0, end_idx - max_show);
                }
                
                // V0.05B9n3e5g3n: 显示上方缩略说明
                int above_count = start_idx;
                if (above_count > 0) {
                    wattron(win, A_DIM);
                    mvwprintw(win, y++, 2, "   ↑ %d more above", above_count);
                    wattroff(win, A_DIM);
                }
                
                for (int i = start_idx; i < end_idx && y < split_y - 2; ++i) {
                    // 当前播放项：使用playlist_index
                    bool is_current = (i == playlist_index);
                    
                    // ═══════════════════════════════════════════════════════════════════
                    // V0.05B9n3e5g3n: 对齐格式
                    // 🔊(2列) + 1空格 = 3列
                    // 3空格 = 3列
                    // 序号从第4列开始，对齐
                    // ═══════════════════════════════════════════════════════════════════
                    std::string prefix;
                    if (is_current) {
                        prefix = "🔊 ";   // 当前播放项：emoji(2列) + 1空格 = 3列
                    } else {
                        prefix = "   ";   // 普通项：3空格 = 3列
                    }
                    
                    // 格式：图标(3列) + 序号 + ". " + 标题
                    std::string title_display = prefix + std::to_string(i + 1) + ". " + 
                        (has_saved_playlist ? saved_playlist[i].title : playlist[i].title);
                    
                    // 高亮当前播放项
                    if (is_current) {
                        wattron(win, COLOR_PAIR(11));  // 绿色
                    }
                    mvwprintw(win, y++, 2, "%s", 
                              Utils::truncate_by_display_width(title_display, safe_cw - 4).c_str());
                    if (is_current) {
                        wattroff(win, COLOR_PAIR(11));
                    }
                }
                
                // V0.05B9n3e5g3n: 显示下方缩略说明
                int below_count = (int)list_size - end_idx;
                if (below_count > 0 && y < split_y - 2) {
                    wattron(win, A_DIM);
                    mvwprintw(win, y++, 2, "   ↓ %d more below", below_count);
                    wattroff(win, A_DIM);
                }
                y++;
            }
            
            // 分隔线
            mvwaddch(win, split_y, 0, ACS_LTEE);
            for (int i = 1; i < right_w - 1; ++i) mvwaddch(win, split_y, i, ACS_HLINE);
            mvwaddch(win, split_y, right_w - 1, ACS_RTEE);
            mvwprintw(win, split_y, 2, " Event Log ");

            // V2.39: EVENT LOG滚动显示，不截断
            auto logs = EventLog::instance().get();
            int current_log_y = border_bottom - 1;
            
            static auto last_scroll_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (scroll_mode_ && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scroll_time).count() > 200) {
                //使用LayoutMetrics统一管理滚动偏移
                layout_.increment_log_scroll_offset();
                last_scroll_time = now;
            }
            
            //使用LayoutMetrics计算日志区域可用净宽度
            int timestamp_width = 14;
            int log_msg_width = layout_.get_log_available_width(timestamp_width);
            
            for (size_t i = 0; i < logs.size() && current_log_y > split_y; ++i) {
                const LogEntry& entry = logs[i];
                
                // V2.39: 滚动显示日志消息
                std::string msg_display;
                int msg_len = Utils::utf8_display_width(entry.message);
                
                if (scroll_mode_ && msg_len > log_msg_width) {
                    //修复中文滚动撞边框问题
                    //使用LayoutMetrics统一管理滚动偏移
                    msg_display = Utils::get_scrolling_text(entry.message, log_msg_width, layout_.get_log_scroll_offset() / 5);
                } else {
                    msg_display = Utils::truncate_by_display_width(entry.message, log_msg_width);
                }
                
                // V2.39: 时间戳 + 空格 + 消息，增加空格对齐
                mvwprintw(win, current_log_y--, 2, "%s %s", 
                          entry.timestamp.c_str(), msg_display.c_str());
            }
            
            //右面板边框保护
            // 标题嵌入区域：列2 到 列14 (" INFO & LOG " = 12字符 + 2空格)
            // 分隔线嵌入区域：列2 到 列13 (" Event Log " = 11字符 + 2空格)
            int right_ww = getmaxx(win);
            int right_wh = getmaxy(win);
            protect_border(win, right_ww, right_wh, 2, 14, split_y, 2, 13);
        }

        //帮助弹窗 - 补充完整键定义
        void draw_help(WINDOW* win, const MPVController::State& state, int cw) {
            (void)win; (void)state; (void)cw; // 保留参数供未来使用
            //帮助内容定义，包含全部键定义
            //更新热键说明
            std::vector<std::string> help_lines = {
                fmt::format("{} {}★ - Help", APP_NAME, VERSION),
                "",
                "---- Navigation ----",
                "  k / j      Move up/down",
                "  g / G      Go to first/last item",
                "  PgUp/PgDn  Page up/down",
                "  h          Collapse/Go back",
                "  l / Enter  Expand/Play (marked: add to playlist)",
                "",
                "---- Playback ----",
                "  Space      Play/Pause",
                "  -          Volume down",
                "  [ / ]      Speed slower/faster",
                "  \\          Reset speed to 1.0x",
                "",
                "---- Playlist ----",
                "  + / =      Add to playlist (marked or current)",
                "  L          Open/Close playlist popup",
                "  Ctrl+L     Toggle theme (dark/light)",
                "",
                "---- Actions ----",
                "  a          Add feed (PODCAST) / Subscribe (ONLINE)",
                "  d          Delete node/record (all modes)",
                "  D          Download episode",
                "  B          Batch download / Switch region (ONLINE)",
                "  e          Edit node title/URL",
                "  f          Add to Favourites",
                "  m          Toggle mark",
                "  v          Enter Visual mode",
                "  V          Clear all marks",
                "  C          Clear playlist (keep playing)",
                "  r          Refresh node",
                "  o          Toggle sort order (asc/desc)",
                "",
                "---- Modes ----",
                "  R          Radio mode",
                "  P          Podcast mode",
                "  F          Favourite mode",
                "  H          History mode",
                "  O          Online (iTunes) mode",
                "  M          Cycle through all modes",
                "",
                "---- UI Settings ----",
                "  T          Toggle tree lines",
                "  S          Toggle scroll mode",
                "  U          Toggle icon style (ASCII/Emoji)",
                "",
                "---- Search ----",
                "  /          Search (local) / Search iTunes (ONLINE)",
                "  J / K      Jump to next/prev match",
                "",
                "---- In Playlist Popup (L) ----",
                "  r          Single play mode (🔂)",
                "  c          Cycle play mode (🔁)",
                "  y          Random play mode (🔀)",
                "  J / K      Move item up/down",
                "  d          Delete selected",
                "  C          Clear all",
                "",
                "---- Command Line ----",
                "  -a <url>   Add feed from URL",
                "  -i <file>  Import OPML subscriptions",
                "  -e <file>  Export podcasts to OPML",
                "  -t <time>  Sleep timer (5h/30m/1:25:15)",
                "  --purge    Clear all cached data",
                "",
                "---- Data Storage ----",
                "  Database: ~/.local/share/podradio/podradio.db",
                "  Config:   ~/.config/podradio/config.ini",
                "  Downloads: ~/Downloads/PodRadio/",
                "  Log:      ~/.local/share/podradio/podradio.log",
                "",
                "  Note: All data in SQLite database.",
                "",
                "---- Contact ----",
                "  Email:  Deadship2003@gmail.com",
                "",
                "Press 'q' or '?' to close"
            };
            
            // V0.03: 计算所需窗口尺寸
            int content_height = help_lines.size();
            int content_width = 0;
            for (const auto& line : help_lines) {
                int w = Utils::utf8_display_width(line) + 4;  // 边框+边距
                if (w > content_width) content_width = w;
            }
            
            // V0.03: 限制最大尺寸为屏幕的90%
            int max_h = (int)(h * 0.9);
            int max_w = (int)(w * 0.9);
            int help_h = std::min(content_height + 2, max_h);  // +2 for border
            int help_w = std::min(content_width, max_w);
            
            // 确保最小尺寸
            if (help_h < 10) help_h = 10;
            if (help_w < 40) help_w = 40;
            
            int help_y = (h - help_h) / 2;
            int help_x = (w - help_w) / 2;
            
            WINDOW* help_win = newwin(help_h, help_w, help_y, help_x);
            keypad(help_win, TRUE);
            
            // 绘制边框
            box(help_win, 0, 0);
            
            // V0.03: 如果内容超出窗口，添加滚动功能
            int scroll_offset = 0;
            bool needs_scroll = content_height > help_h - 2;
            
            auto draw_content = [&]() {
                werase(help_win);
                box(help_win, 0, 0);
                int y = 1;
                int visible_lines = help_h - 2;
                
                for (int i = scroll_offset; i < content_height && y < help_h - 1; ++i) {
                    const std::string& line = help_lines[i];
                    std::string display = Utils::truncate_by_display_width(line, help_w - 4);
                    
                    if (i == 0) {
                        //标题居中显示
                        int title_width = Utils::utf8_display_width(display);
                        int x_pos = (help_w - title_width) / 2;
                        if (x_pos < 2) x_pos = 2;
                        wattron(help_win, A_BOLD);
                        mvwprintw(help_win, y++, x_pos, "%s", display.c_str());
                        wattroff(help_win, A_BOLD);
                    } else if (line.find("----") == 0) {
                        // 分隔标题
                        wattron(help_win, A_DIM);
                        mvwprintw(help_win, y++, 2, "%s", display.c_str());
                        wattroff(help_win, A_DIM);
                    } else if (line == "Press 'q' or '?' to close") {
                        // 底部提示
                        wattron(help_win, A_DIM);
                        mvwprintw(help_win, y++, 2, "%s", display.c_str());
                        wattroff(help_win, A_DIM);
                    } else {
                        mvwprintw(help_win, y++, 2, "%s", display.c_str());
                    }
                }
                
                // V0.03: 显示滚动指示器
                if (needs_scroll) {
                    if (scroll_offset > 0) {
                        mvwprintw(help_win, 0, help_w - 6, "▲");
                    }
                    if (scroll_offset + visible_lines < content_height) {
                        mvwprintw(help_win, help_h - 1, help_w - 6, "▼");
                    }
                }
                
                wrefresh(help_win);
            };
            
            draw_content();
            
            // V0.03: 支持滚动的按键处理
            //添加g/G跳头跳尾
            int ch;
            while ((ch = wgetch(help_win)) != 'q' && ch != '?' && ch != 27) {
                if (ch == 'k' || ch == KEY_UP) {
                    if (scroll_offset > 0) {
                        scroll_offset--;
                        draw_content();
                    }
                } else if (ch == 'j' || ch == KEY_DOWN) {
                    if (scroll_offset + help_h - 2 < content_height) {
                        scroll_offset++;
                        draw_content();
                    }
                } else if (ch == KEY_PPAGE) {
                    scroll_offset = std::max(0, scroll_offset - 5);
                    draw_content();
                } else if (ch == KEY_NPAGE) {
                    scroll_offset = std::min(content_height - help_h + 2, scroll_offset + 5);
                    draw_content();
                } else if (ch == 'g') {
                    //跳到顶部
                    scroll_offset = 0;
                    draw_content();
                } else if (ch == 'G') {
                    //跳到底部
                    scroll_offset = std::max(0, content_height - help_h + 2);
                    draw_content();
                }
            }
            
            delwin(help_win);
        }
    };

    // =========================================================
    // 主应用
    // =========================================================
    class App {
    public:
        App() {
            Logger::instance().init();
            IniConfig::instance().load();
            // V0.03: 初始化SQLite3数据库
            DatabaseManager::instance().init();
            CacheManager::instance();
            YouTubeCache::instance().load();
            
            //将根节点初始化移至构造函数，避免命令行模式下未初始化
            // 这样在export_podcasts()调用时podcast_root已有效
            radio_root = std::make_shared<TreeNode>();
            radio_root->title = "Root";
            radio_root->type = NodeType::FOLDER;
            radio_root->expanded = true;
            
            podcast_root = std::make_shared<TreeNode>();
            podcast_root->title = "Root";
            podcast_root->type = NodeType::FOLDER;
            podcast_root->expanded = true;
            
            fav_root = std::make_shared<TreeNode>();
            fav_root->title = "Root";
            fav_root->type = NodeType::FOLDER;
            fav_root->expanded = true;
            
            history_root = std::make_shared<TreeNode>();
            history_root->title = "Root";
            history_root->type = NodeType::FOLDER;
            history_root->expanded = true;
            
            current_root = radio_root;
        }
        
        void run() {
            ui.init();
            //确认XML错误处理函数已设置（main中已设置，此处为二次确认）
            // 将错误重定向到LOG/EVENT_LOG，避免输出到终端导致花屏
            xmlSetGenericErrorFunc(NULL, xml_error_handler);
            xmlSetStructuredErrorFunc(NULL, xml_structured_error_handler);
            if (!player.initialize()) LOG("MPV initialization failed");
            
            //注册播放结束回调，实现自动播放下一曲
            player.set_end_file_callback([this](int reason) {
                on_playback_ended(reason);
            });
            LOG("[AUTOPLAY] End-file callback registered");
            
            current_root = radio_root;
            
            Persistence::load_cache(radio_root, podcast_root);
            load_persistent_data();
            
            //加载历史记录到history_root
            load_history_to_root();
            
            //恢复上次播放状态
            restore_player_state();
            
            mark_cached_nodes(radio_root);
            mark_cached_nodes(podcast_root);
            
            if (radio_root->children.empty()) {
                std::thread([this]() { load_radio_root(); }).detach();
            }
            // V2.39-FF: 始终加载内置播客，与缓存数据合并
            load_default_podcasts();

            //初始化布局管理器
            LayoutMetrics::instance().check_resize();

            while (running) {
                //检查SIGINT(CTRL+C)退出请求
                if (g_exit_requested) {
                    g_exit_requested = false;  // 重置标志
                    if (ui.confirm_box("Quit PODRADIO? (CTRL+C)")) {
                        running = false;
                        break;
                    }
                }
                
                //检查睡眠定时器
                if (SleepTimer::instance().is_active() && SleepTimer::instance().check_expired()) {
                    EVENT_LOG("Sleep timer expired, exiting...");
                    running = false;
                    break;
                }
                
                //统一窗口尺寸检测与滚动偏移重置
                // LayoutMetrics自动检测尺寸变化并重置所有滚动偏移
                if (LayoutMetrics::instance().check_resize()) {
                    // 强制重新调整窗口大小
                    resizeterm(LINES, COLS);
                    // 清除屏幕并触发完全重绘
                    clear();
                    refresh();
                    EVENT_LOG(fmt::format("Terminal resized: {}x{}", COLS, LINES));
                }
                
                auto state = player.get_state();

                // V0.05B9n3e5g3k: 修复SINGLE模式下的光标同步问题
                // SINGLE模式：MPV列表只有一首歌，playlist_pos始终为0
                // 但current_playlist_index应该保持用户选择的值
                // CYCLE/RANDOM模式：需要根据MPV位置更新光标
                if (state.has_media && !current_playlist.empty() && 
                    state.playlist_pos >= 0 && state.playlist_pos < (int)current_playlist.size()) {
                    
                    int new_index = -1;
                    
                    if (play_mode == PlayMode::RANDOM && !shuffle_map_.empty() &&
                        state.playlist_pos < (int)shuffle_map_.size()) {
                        // RANDOM模式：使用映射将MPV位置转换为原始列表位置
                        new_index = static_cast<int>(shuffle_map_[state.playlist_pos]);
                    } else if (play_mode == PlayMode::CYCLE) {
                        // CYCLE模式：直接使用MPV位置
                        new_index = state.playlist_pos;
                    }
                    // SINGLE模式：不更新current_playlist_index
                    // 因为在play_saved_playlist_item中已正确设置为用户选择的idx
                    
                    if (new_index >= 0 && current_playlist_index != new_index) {
                        current_playlist_index = new_index;
                        // V0.05B9n3e5g3k: 同步 L模式光标到当前播放项
                        // 播放时L弹窗和INFO&LOG区域的列表光标保持一致
                        if (!saved_playlist.empty() && new_index >= 0 && new_index < (int)saved_playlist.size()) {
                            list_selected_idx = new_index;
                        }
                        LOG(fmt::format("[SYNC] Playlist index synced to {} (mpv_pos={}, mode={})", 
                            current_playlist_index, state.playlist_pos, 
                            play_mode == PlayMode::RANDOM ? "RANDOM" : "CYCLE"));
                    }
                }

                // V0.03a: 改进状态判断逻辑，正确显示Navigating/Buffering/Playing/Pause
                //添加List模式检测
                AppState app_state;
                if (in_list_mode_) {
                    //List模式优先
                    app_state = AppState::LIST_MODE;
                } else if (state.has_media) {
                    if (state.core_idle && !state.paused) {
                        // 有媒体，空闲且未暂停 = 缓冲中
                        app_state = AppState::BUFFERING;
                    } else if (state.paused) {
                        // 有媒体，暂停
                        app_state = AppState::PAUSED;
                    } else {
                        // 有媒体，正在播放
                        app_state = AppState::PLAYING;
                    }
                } else {
                    // 无媒体，浏览中
                    app_state = AppState::BROWSING;
                }

                bool is_loading = false;
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    display_list.clear();
                    flatten(current_root, 0, true, -1);
                    for (const auto& item : display_list) {
                        if (item.node->loading) { is_loading = true; break; }
                    }
                }
                // 节点加载中的状态优先级高于浏览，但低于播放状态
                if (is_loading && app_state == AppState::BROWSING) app_state = AppState::LOADING;

                int marked = count_marked_safe(current_root);
                TreeNodePtr sel_node = (selected_idx >= 0 && selected_idx < (int)display_list.size()) ?
                    display_list[selected_idx].node : nullptr;
                auto downloads = ProgressManager::instance().get_all();

                int vh = LINES - 5;
                if (vh < 1) vh = 1;
                if (selected_idx < view_start) view_start = selected_idx;
                else if (selected_idx >= view_start + vh) view_start = selected_idx - vh + 1;
                if (view_start < 0) view_start = 0;

                // V0.05B9n3e5g3j: 同步L列表光标到当前播放项
                sync_list_cursor_from_mpv(state);

                //List模式下使用特殊的绘制调用
                ui.draw(mode, display_list, selected_idx, state, view_start, app_state,
                        playback_node, marked, search_query, current_match_idx, total_matches,
                        sel_node, downloads, visual_mode_, visual_start_,
                        current_playlist, current_playlist_index,
                        saved_playlist, list_selected_idx, list_marked_indices,
                        play_mode);

                int ch = getch();
                if (ch != ERR) handle_input(ch, marked);
            }
            
            save_persistent_data();
            Persistence::save_cache(radio_root, podcast_root);
            CacheManager::instance().save();
            
            //保存播放器状态
            auto player_state = player.get_state();
            std::string current_title = playback_node ? playback_node->title : "";
            int mode_int = static_cast<int>(mode);
            DatabaseManager::instance().save_player_state(
                player_state.volume,
                player_state.speed,
                player_state.paused,
                player_state.current_url,
                player_state.time_pos,
                ui.is_scroll_mode(),
                true,  // show_tree_lines
                current_title,
                mode_int
            );
            EVENT_LOG("Player state saved");
            
            ui.cleanup();
        }
        
        //使用std::cout替代EVENT_LOG，兼容命令行模式
        void import_feed(const std::string& url) {
            std::cout << "Adding feed: " << url << std::endl;
            
            auto node = std::make_shared<TreeNode>();
            node->url = url;
            node->type = NodeType::PODCAST_FEED;
            node->title = URLClassifier::extract_channel_name(url);
            
            podcast_root->children.insert(podcast_root->children.begin(), node);
            spawn_load_feed(node);
            
            Persistence::save_cache(radio_root, podcast_root);
            Persistence::save_data(podcast_root->children, fav_root->children);
            std::cout << "Feed added successfully: " << node->title << std::endl;
        }
        
        //从OPML文件导入，使用std::cout替代EVENT_LOG
        void import_opml(const std::string& filepath) {
            std::cout << "Importing from: " << filepath << std::endl;
            
            auto feeds = OPMLParser::import_opml_file(filepath);
            if (feeds.empty()) {
                std::cout << "No feeds found in OPML file" << std::endl;
                return;
            }
            
            std::cout << "Found " << feeds.size() << " feeds in OPML" << std::endl;

            int count = 0;
            for (auto& feed : feeds) {
                // 检查是否已存在
                bool exists = false;
                for (const auto& existing : podcast_root->children) {
                    if (existing->url == feed->url) {
                        exists = true;
                        std::cout << "  Skipping duplicate: " << feed->title << std::endl;
                        break;
                    }
                }

                if (!exists) {
                    podcast_root->children.insert(podcast_root->children.begin(), feed);
                    spawn_load_feed(feed);
                    count++;
                    std::cout << "  Added: " << (feed->title.empty() ? feed->url : feed->title) << std::endl;
                }
            }

            std::cout << "Imported " << count << " new feeds" << std::endl;
            Persistence::save_cache(radio_root, podcast_root);
            Persistence::save_data(podcast_root->children, fav_root->children);
        }
        
        //导出播客订阅
        void export_podcasts(const std::string& filename) {
            Persistence::export_opml(filename, podcast_root->children);
        }
        
        //公开方法供命令行模式加载持久化数据
        void load_data() {
            Persistence::load_cache(radio_root, podcast_root);
            load_persistent_data();
            std::cout << "Loaded " << podcast_root->children.size() << " podcasts from cache" << std::endl;
        }
        
    private:
        UI ui;
        MPVController player;
        TreeNodePtr radio_root, podcast_root, fav_root, history_root, current_root, playback_node;  //添加history_root
        std::vector<DisplayItem> display_list;
        int selected_idx = 0, view_start = 0;
        bool running = true;
        AppMode mode = AppMode::RADIO;
        std::recursive_mutex tree_mutex;
        std::string search_query;
        std::vector<TreeNodePtr> search_matches;
        int current_match_idx = -1, total_matches = 0;
        bool visual_mode_ = false;
        int visual_start_ = -1;
        
        //当前播放列表
        std::vector<PlaylistItem> current_playlist;
        int current_playlist_index = -1;
        
        // V0.05B9n3e5g3h: 随机播放映射
        // shuffle_map_[mpv_pos] = original_index
        // 用于RANDOM模式下将MPV播放位置映射回原始列表位置
        std::vector<size_t> shuffle_map_;

        // ═════════════════════════════════════════════════════════════════════════
        //List模式（播放列表管理模式）
        //改为弹窗模式，支持完整快捷键
        // ═════════════════════════════════════════════════════════════════════════
        std::vector<SavedPlaylistItem> saved_playlist;  // 持久化的播放列表
        int list_selected_idx = 0;  // List模式中选中的索引
        bool in_list_mode_ = false;  // 是否处于List模式
        std::set<int> list_marked_indices;  //List模式标记索引
        bool list_visual_mode_ = false;  //List模式Visual选择
        int list_visual_start_ = -1;  //List模式Visual起点
        std::string list_search_filter_;  //List模式搜索过滤
        PlayMode play_mode = PlayMode::SINGLE;  //播放模式

        // V0.05B9n3e5g3j: 光标同步追踪
        // 记录上次的MPV播放位置，用于检测播放位置变化
        int last_mpv_playlist_pos_ = -1;

        // ═════════════════════════════════════════════════════════════════════════
        // V0.05B9n3e5g3j: 从MPV状态同步L列表光标
        // 当播放位置变化时，自动将L列表光标移动到当前播放项
        // ═════════════════════════════════════════════════════════════════════════
        void sync_list_cursor_from_mpv(const MPVController::State& state) {
            // 只有在saved_playlist不为空且正在播放时才同步
            if (saved_playlist.empty() || !state.has_media) return;

            int mpv_pos = state.playlist_pos;

            // 检测播放位置是否变化
            if (mpv_pos == last_mpv_playlist_pos_) return;
            last_mpv_playlist_pos_ = mpv_pos;

            // 根据播放模式计算对应的saved_playlist索引
            int target_idx = -1;

            switch (play_mode) {
                case PlayMode::SINGLE:
                    // 单曲模式：光标始终在当前播放项
                    target_idx = current_playlist_index;
                    break;

                case PlayMode::CYCLE:
                    // 循环模式：MPV的playlist_pos直接对应current_playlist的索引
                    // current_playlist保持了saved_playlist的顺序
                    if (mpv_pos >= 0 && mpv_pos < static_cast<int>(current_playlist.size())) {
                        target_idx = mpv_pos;
                        // 更新current_playlist_index
                        current_playlist_index = target_idx;
                    }
                    break;

                case PlayMode::RANDOM:
                    // 随机模式：需要通过shuffle_map_反推原始索引
                    // shuffle_map_[mpv_pos] = original_index
                    if (mpv_pos >= 0 && mpv_pos < static_cast<int>(shuffle_map_.size())) {
                        target_idx = static_cast<int>(shuffle_map_[mpv_pos]);
                        current_playlist_index = target_idx;
                    }
                    break;
            }

            // 更新L列表光标
            if (target_idx >= 0 && target_idx < static_cast<int>(saved_playlist.size())) {
                if (list_selected_idx != target_idx) {
                    list_selected_idx = target_idx;
                    LOG(fmt::format("[SYNC] Cursor synced to idx {}", target_idx));
                }
            }
        }

        // ═════════════════════════════════════════════════════════════════════════
        //自动播放下一项功能
        // ═════════════════════════════════════════════════════════════════════════
        
        // 检查节点是否可播放
        // V0.05B9n3e5g3b修复：只要有URL就可以播放
        bool is_playable_node(TreeNodePtr node) {
            if (!node) return false;
            return !node->url.empty();
        }
        
        // 查找下一个可播放的兄弟节点
        //播放列表结束后查找兄弟节点继续播放
        TreeNodePtr find_next_playable_sibling(TreeNodePtr current) {
            if (!current) return nullptr;
            
            //parent是weak_ptr，需要lock()获取shared_ptr
            auto parent = current->parent.lock();
            if (!parent) return nullptr;
            
            auto& siblings = parent->children;
            
            // 在父节点的子节点中找到当前节点
            size_t current_idx = 0;
            for (size_t i = 0; i < siblings.size(); ++i) {
                if (siblings[i].get() == current.get()) {
                    current_idx = i;
                    break;
                }
            }
            
            // 根据父节点的sort_reversed状态决定遍历方向
            // sort_reversed=true表示倒序(新→旧)，下一个应该是索引-1
            // sort_reversed=false表示正序(旧→新)，下一个应该是索引+1
            int direction = parent->sort_reversed ? -1 : 1;
            int next_idx = static_cast<int>(current_idx) + direction;
            
            // 查找下一个可播放节点
            while (next_idx >= 0 && next_idx < static_cast<int>(siblings.size())) {
                if (is_playable_node(siblings[next_idx])) {
                    return siblings[next_idx];
                }
                next_idx += direction;
            }
            
            return nullptr;
        }
        
        // 播放结束回调处理
        // V0.05B9n3e5g3b: 简化逻辑，MPV播放列表自动处理下一项
        // 此函数主要处理列表结束后的循环/随机播放
        void on_playback_ended(int reason) {
            // 只处理正常结束(reason=0)
            if (reason != 0) {
                LOG(fmt::format("[AUTOPLAY] End file reason={}, ignored", reason));
                return;
            }

            // 获取MPV播放列表状态
            auto state = player.get_state();
            int pl_pos = state.playlist_pos;
            int pl_count = state.playlist_count;

            LOG(fmt::format("[AUTOPLAY] Track ended, playlist_pos={}, count={}", pl_pos, pl_count));

            // 判断是否是播放列表最后一项
            // 如果playlist_pos不是最后一项，说明MPV会自动播放下一项，不需要干预
            // 注意：END_FILE触发时，playlist-pos可能已经是-1（列表结束）或下一项的位置
            bool is_last_item = (pl_count <= 1) || (pl_pos < 0) || (pl_pos >= pl_count - 1);

            if (!is_last_item) {
                // MPV会自动播放下一项，不需要干预
                LOG("[AUTOPLAY] MPV will auto-play next item");
                return;
            }

            // 播放列表结束，根据播放模式决定是否继续
            LOG("[AUTOPLAY] Playlist ended, checking mode...");

            switch (play_mode) {
                case PlayMode::SINGLE:
                    // SINGLE模式：播放结束，不循环
                    EVENT_LOG("Playlist ended (SINGLE mode)");
                    LOG("[AUTOPLAY] SINGLE mode: playback ended");
                    break;

                case PlayMode::CYCLE:
                    // CYCLE模式：从头循环
                    if (!current_playlist.empty()) {
                        EVENT_LOG("Cycle: restarting playlist");
                        LOG("[AUTOPLAY] CYCLE mode: restarting");
                        // 重新构建播放列表并播放
                        std::vector<std::string> urls;
                        bool has_video = false;
                        for (const auto& item : current_playlist) {
                            urls.push_back(item.url);
                            if (item.is_video) has_video = true;
                        }
                        current_playlist_index = 0;
                        player.play_list(urls, has_video);
                    }
                    break;

                case PlayMode::RANDOM:
                    // RANDOM模式：重新随机
                    if (!current_playlist.empty()) {
                        EVENT_LOG("Random: reshuffling playlist");
                        LOG("[AUTOPLAY] RANDOM mode: reshuffling");

                        // 随机打乱
                        std::random_device rd;
                        std::mt19937 gen(rd());
                        std::vector<size_t> indices;
                        for (size_t i = 0; i < current_playlist.size(); ++i) {
                            indices.push_back(i);
                        }
                        std::shuffle(indices.begin(), indices.end(), gen);

                        // 重建播放列表
                        std::vector<PlaylistItem> shuffled;
                        std::vector<std::string> urls;
                        bool has_video = false;
                        for (size_t i : indices) {
                            shuffled.push_back(current_playlist[i]);
                            urls.push_back(current_playlist[i].url);
                            if (current_playlist[i].is_video) has_video = true;
                        }
                        current_playlist = shuffled;
                        current_playlist_index = 0;
                        player.play_list(urls, has_video);
                    }
                    break;
            }
        }

        // 清除播放列表（当前播放继续）
        //按C键清除播放列表
        void clear_playlist() {
            int count = current_playlist.size();
            current_playlist.clear();
            current_playlist_index = -1;
            EVENT_LOG(fmt::format("Cleared playlist ({} items)", count));
            LOG(fmt::format("[PLAYLIST] Cleared {} items, current playback continues", count));
        }

        // ═════════════════════════════════════════════════════════════════════════
        //List模式（播放列表管理模式）
        // ═════════════════════════════════════════════════════════════════════════

        // 处理后的播放列表结构（必须在apply_play_mode_for_list之前定义）
        struct ProcessedPlaylist {
            std::vector<std::string> urls;    // 处理后的URL列表
            int start_idx;                     // 起始播放位置
            bool loop_single;                  // 是否单曲循环（R模式）
        };

        // 加载持久化的播放列表
        void load_saved_playlist() {
            saved_playlist = DatabaseManager::instance().load_playlist();
            if (list_selected_idx >= static_cast<int>(saved_playlist.size())) {
                list_selected_idx = std::max(0, static_cast<int>(saved_playlist.size()) - 1);
            }
            LOG(fmt::format("[LIST] Loaded {} saved playlist items", saved_playlist.size()));
        }

        // 保存当前播放列表到数据库
        void save_current_playlist_to_db() {
            // 先清空数据库中的播放列表
            DatabaseManager::instance().clear_playlist_table();

            // 保存当前播放列表
            for (size_t i = 0; i < current_playlist.size(); ++i) {
                SavedPlaylistItem item;
                item.title = current_playlist[i].title;
                item.url = current_playlist[i].url;
                item.duration = current_playlist[i].duration;
                item.is_video = current_playlist[i].is_video;
                item.node_path = current_playlist[i].node_path;
                item.sort_order = static_cast<int>(i);
                DatabaseManager::instance().save_playlist_item(item);
            }
            EVENT_LOG(fmt::format("Saved {} items to playlist", current_playlist.size()));
            LOG(fmt::format("[LIST] Saved {} items to database", current_playlist.size()));

            // 重新加载以获取ID
            load_saved_playlist();
        }

        // 添加当前选中项到播放列表
        void add_to_saved_playlist(TreeNodePtr node) {
            if (!node) return;

            SavedPlaylistItem item;
            item.title = node->title;
            item.url = node->url;
            item.duration = node->duration;
            URLType url_type = URLClassifier::classify(node->url);
            item.is_video = node->is_youtube || URLClassifier::is_video(url_type);
            item.node_path = node->url;  // 使用URL作为节点路径标识
            item.sort_order = static_cast<int>(saved_playlist.size());

            DatabaseManager::instance().save_playlist_item(item);
            saved_playlist.push_back(item);

            EVENT_LOG(fmt::format("Added to playlist: {}", node->title));
            LOG(fmt::format("[LIST] Added: {}", node->url));
        }

        // 从播放列表中删除选中项
        void delete_from_saved_playlist(int idx) {
            if (idx < 0 || idx >= static_cast<int>(saved_playlist.size())) return;

            std::string title = saved_playlist[idx].title;
            int id = saved_playlist[idx].id;

            DatabaseManager::instance().delete_playlist_item(id);
            saved_playlist.erase(saved_playlist.begin() + idx);

            // 重新排序
            DatabaseManager::instance().reorder_playlist();
            load_saved_playlist();

            // 调整选中索引
            if (list_selected_idx >= static_cast<int>(saved_playlist.size())) {
                list_selected_idx = std::max(0, static_cast<int>(saved_playlist.size()) - 1);
            }

            EVENT_LOG(fmt::format("Removed from playlist: {}", title));
            LOG(fmt::format("[LIST] Deleted item at index {}", idx));
        }

        // 清空播放列表（数据库）
        void clear_saved_playlist() {
            int count = saved_playlist.size();
            DatabaseManager::instance().clear_playlist_table();
            saved_playlist.clear();
            list_selected_idx = 0;
            EVENT_LOG(fmt::format("Cleared saved playlist ({} items)", count));
            LOG("[LIST] Cleared all saved playlist items");
        }

        // 上移播放列表项
        void move_playlist_item_up(int idx) {
            if (idx <= 0 || idx >= static_cast<int>(saved_playlist.size())) return;

            // 交换排序顺序
            int id1 = saved_playlist[idx].id;
            int id2 = saved_playlist[idx - 1].id;
            int order1 = saved_playlist[idx].sort_order;
            int order2 = saved_playlist[idx - 1].sort_order;

            DatabaseManager::instance().update_playlist_order(id1, order2);
            DatabaseManager::instance().update_playlist_order(id2, order1);

            // 重新加载
            load_saved_playlist();
            list_selected_idx = idx - 1;

            EVENT_LOG("Moved playlist item up");
        }

        // 下移播放列表项
        void move_playlist_item_down(int idx) {
            if (idx < 0 || idx >= static_cast<int>(saved_playlist.size()) - 1) return;

            // 交换排序顺序
            int id1 = saved_playlist[idx].id;
            int id2 = saved_playlist[idx + 1].id;
            int order1 = saved_playlist[idx].sort_order;
            int order2 = saved_playlist[idx + 1].sort_order;

            DatabaseManager::instance().update_playlist_order(id1, order2);
            DatabaseManager::instance().update_playlist_order(id2, order1);

            // 重新加载
            load_saved_playlist();
            list_selected_idx = idx + 1;

            EVENT_LOG("Moved playlist item down");
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // List模式播放 - 使用模块化架构
        // ═══════════════════════════════════════════════════════════════════════════
        
        // 播放列表中的选中项
        // V0.05B9n3e5g3j: 使用完整播放列表（不截断），支持R/C/Y三种播放模式
        // 光标同步：播放时同步L列表光标到当前播放项
        void play_saved_playlist_item(int idx) {
            if (idx < 0 || idx >= static_cast<int>(saved_playlist.size())) return;

            const auto& item = saved_playlist[idx];

            //检查URL有效性
            if (item.url.empty()) {
                EVENT_LOG("Cannot play: URL is empty");
                return;
            }

            // 1. 构建完整播放列表
            std::vector<std::string> playlist_urls;
            current_playlist.clear();
            bool has_video = false;
            
            for (size_t i = 0; i < saved_playlist.size(); ++i) {
                const auto& si = saved_playlist[i];
                
                // 检查本地缓存
                std::string local = CacheManager::instance().get_local_file(si.url);
                std::string play_url;
                if (!local.empty() && fs::exists(local)) {
                    play_url = "file://" + local;
                } else {
                    play_url = si.url;
                }
                
                playlist_urls.push_back(play_url);
                
                PlaylistItem pl_item;
                pl_item.title = si.title;
                pl_item.url = play_url;
                pl_item.duration = si.duration;
                pl_item.is_video = si.is_video;
                pl_item.node_path = si.node_path;
                current_playlist.push_back(pl_item);
                
                if (si.is_video) has_video = true;
            }
            
            // 2. 应用播放模式处理
            auto processed = apply_play_mode_for_list(playlist_urls, idx, play_mode);
            
            // 3. 根据播放模式设置 current_playlist_index
            // V0.05B9n3e5g3j: 光标同步到当前播放项
            switch (play_mode) {
                case PlayMode::SINGLE:
                    // 单曲模式：光标指向用户选择的项
                    current_playlist_index = idx;
                    list_selected_idx = idx;  // 光标同步
                    // 清空shuffle_map_
                    shuffle_map_.clear();
                    break;
                case PlayMode::CYCLE:
                    // 循环模式：光标指向用户选择的项
                    current_playlist_index = idx;
                    list_selected_idx = idx;  // 光标同步
                    // 清空shuffle_map_
                    shuffle_map_.clear();
                    break;
                case PlayMode::RANDOM:
                    // 随机模式：current_playlist_index已在apply_play_mode_for_list中设置
                    // shuffle_map_也已设置
                    // 光标同步：list_selected_idx指向原始列表中对应的项
                    if (!shuffle_map_.empty() && current_playlist_index >= 0) {
                        list_selected_idx = current_playlist_index;
                    }
                    break;
            }
            
            // 4. 执行播放
            execute_play_for_list(processed.urls, processed.start_idx, has_video, processed.loop_single);
            
            playback_node = nullptr;  // List模式下没有树节点引用
            
            EVENT_LOG(fmt::format("L-List Play: idx {}, cursor synced", idx));
        }
        
        // List模式专用的播放模式处理
        ProcessedPlaylist apply_play_mode_for_list(const std::vector<std::string>& urls, int current_idx, PlayMode mode) {
            ProcessedPlaylist result;
            result.loop_single = false;
            
            if (urls.empty()) {
                result.start_idx = 0;
                return result;
            }
            
            // 确保current_idx在有效范围内
            if (current_idx < 0) current_idx = 0;
            if (current_idx >= static_cast<int>(urls.size())) current_idx = urls.size() - 1;
            
            switch (mode) {
                case PlayMode::SINGLE: {
                    // R (Repeat) 模式：单曲循环
                    // 列表只包含当前曲目，设置循环播放
                    result.urls.push_back(urls[current_idx]);
                    result.start_idx = 0;
                    result.loop_single = true;
                    EVENT_LOG(fmt::format("List Mode R (Repeat): single track loop - '{}'", 
                        current_idx < static_cast<int>(current_playlist.size()) ? 
                        current_playlist[current_idx].title : "unknown"));
                    break;
                }
                
                case PlayMode::CYCLE: {
                    // C (Cycle) 模式：顺序播放完整列表
                    // 使用完整列表，从当前项开始播放
                    result.urls = urls;
                    result.start_idx = current_idx;
                    EVENT_LOG(fmt::format("List Mode C (Cycle): {} items total, start from idx {}", 
                        result.urls.size(), current_idx));
                    break;
                }
                
                case PlayMode::RANDOM: {
                    // Y (Random) 模式：随机播放
                    // V0.05B9n3e5g3h: 列表顺序保持不变，只有播放顺序随机
                    // 光标（指针）指向当前播放的项
                    
                    // 生成随机索引
                    std::vector<size_t> indices;
                    for (size_t i = 0; i < urls.size(); ++i) {
                        indices.push_back(i);
                    }
                    
                    // 随机打乱索引
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::shuffle(indices.begin(), indices.end(), gen);
                    
                    // 构建shuffle_map_: shuffle_map_[mpv_pos] = original_index
                    // 这样可以通过MPV播放位置找到原始列表位置
                    shuffle_map_.clear();
                    shuffle_map_.resize(indices.size());
                    for (size_t mpv_pos = 0; mpv_pos < indices.size(); ++mpv_pos) {
                        shuffle_map_[mpv_pos] = indices[mpv_pos];
                    }
                    
                    // 按打乱顺序构建URL列表给MPV播放
                    for (size_t i : indices) {
                        result.urls.push_back(urls[i]);
                    }
                    result.start_idx = 0;
                    
                    // current_playlist保持原始顺序不变！
                    // current_playlist_index指向第一首播放的项（打乱后的第一首对应原始列表中的哪一项）
                    current_playlist_index = static_cast<int>(shuffle_map_[0]);
                    
                    EVENT_LOG(fmt::format("List Mode Y (Random): {} items, first playing idx {}", 
                        result.urls.size(), current_playlist_index));
                    break;
                }
            }
            
            return result;
        }
        
        // List模式专用的播放执行
        void execute_play_for_list(const std::vector<std::string>& urls, int start_idx, bool has_video, bool loop_single) {
            if (urls.empty()) return;
            
            // 设置循环模式
            if (loop_single) {
                // R模式：单曲循环
                player.set_loop_file(true);
                player.set_loop_playlist(false);
            } else {
                // C/Y模式：取消单曲循环
                player.set_loop_file(false);
            }
            
            // 执行播放
            player.play_list_from(urls, start_idx, has_video);
            
            LOG(fmt::format("[LIST] Executed: {} items, start_idx={}, loop_single={}", 
                urls.size(), start_idx, loop_single));
        }

        void mark_cached_nodes(TreeNodePtr node) {
            if (CacheManager::instance().is_cached(node->url)) {
                node->is_cached = true;
            }
            if (CacheManager::instance().is_downloaded(node->url)) {
                node->is_downloaded = true;
                std::string local = CacheManager::instance().get_local_file(node->url);
                if (!local.empty()) node->local_file = local;
            }
            for (auto& child : node->children) {
                mark_cached_nodes(child);
            }
        }

        void handle_input(int ch, int marked_count) {
            //List模式弹窗的完整按键处理
            if (in_list_mode_) {
                int visible_lines = std::min(23, static_cast<int>(saved_playlist.size()));

                switch (ch) {
                    // ═══════════════════════════════════════════════════════════
                    // 导航按键
                    // ═══════════════════════════════════════════════════════════
                    case 'j':  // 下移选择
                        if (list_selected_idx < static_cast<int>(saved_playlist.size()) - 1) {
                            list_selected_idx++;
                        }
                        break;
                    case 'k':  // 上移选择
                        if (list_selected_idx > 0) {
                            list_selected_idx--;
                        }
                        break;
                    case 'g':  // 跳到顶部
                        list_selected_idx = 0;
                        break;
                    case 'G':  // 跳到底部
                        if (!saved_playlist.empty()) {
                            list_selected_idx = static_cast<int>(saved_playlist.size()) - 1;
                        }
                        break;
                    case KEY_PPAGE:  // PageUp - 向上翻页
                    case 2:  // Ctrl+B
                        list_selected_idx = std::max(0, list_selected_idx - visible_lines);
                        break;
                    case KEY_NPAGE:  // PageDown - 向下翻页
                    case 6:  // Ctrl+F
                        list_selected_idx = std::min(static_cast<int>(saved_playlist.size()) - 1,
                                                     list_selected_idx + visible_lines);
                        break;

                    // ═══════════════════════════════════════════════════════════
                    // 编辑操作
                    // ═══════════════════════════════════════════════════════════
                    case 'd':  // 删除选中项
                        if (list_visual_mode_) {
                            // Visual模式：删除所有标记项
                            std::vector<int> to_delete(list_marked_indices.begin(), list_marked_indices.end());
                            std::sort(to_delete.rbegin(), to_delete.rend());  // 从后往前删除
                            for (int idx : to_delete) {
                                delete_from_saved_playlist(idx);
                            }
                            list_marked_indices.clear();
                            list_visual_mode_ = false;
                            EVENT_LOG(fmt::format("Deleted {} marked items", to_delete.size()));
                        } else {
                            delete_from_saved_playlist(list_selected_idx);
                        }
                        break;
                    case 'J':  // 下移排序
                        move_playlist_item_down(list_selected_idx);
                        break;
                    case 'K':  // 上移排序
                        move_playlist_item_up(list_selected_idx);
                        break;
                    case 'C':  // 清空播放列表
                        if (ui.confirm_box("Clear entire playlist?")) {
                            clear_saved_playlist();
                        }
                        break;

                    // ═══════════════════════════════════════════════════════════
                    // 播放模式切换（仅在L弹窗内有效）
                    //r=单次, c=循环, y=随机
                    // ═══════════════════════════════════════════════════════════
                    case 'r':
                    case 'R':
                        play_mode = PlayMode::SINGLE;
                        EVENT_LOG("Play mode: 🔂 Single");
                        break;
                    case 'c':
                        play_mode = PlayMode::CYCLE;
                        EVENT_LOG("Play mode: 🔁 Cycle");
                        break;
                    case 'y':
                    case 'Y':
                        play_mode = PlayMode::RANDOM;
                        EVENT_LOG("Play mode: 🔀 Random");
                        break;

                    // ═══════════════════════════════════════════════════════════
                    // 标记和Visual模式
                    // ═══════════════════════════════════════════════════════════
                    case 'm':  // 标记/取消标记
                        if (list_marked_indices.count(list_selected_idx)) {
                            list_marked_indices.erase(list_selected_idx);
                        } else {
                            list_marked_indices.insert(list_selected_idx);
                        }
                        // 自动移到下一项
                        if (list_selected_idx < static_cast<int>(saved_playlist.size()) - 1) {
                            list_selected_idx++;
                        }
                        break;
                    case 'v':  // 进入/确认 Visual模式
                        if (!list_visual_mode_) {
                            list_visual_mode_ = true;
                            list_visual_start_ = list_selected_idx;
                            list_marked_indices.insert(list_selected_idx);
                        } else {
                            // 确认选择，保留标记
                            list_visual_mode_ = false;
                        }
                        break;
                    case 'V':  // 取消Visual模式/清除所有标记
                        list_visual_mode_ = false;
                        list_marked_indices.clear();
                        EVENT_LOG("Cleared all marks");
                        break;

                    // ═══════════════════════════════════════════════════════════
                    // 播放操作
                    // ═══════════════════════════════════════════════════════════
                    case '\n':  // Enter - 播放选中项
                    case 'l':
                        play_saved_playlist_item(list_selected_idx);
                        in_list_mode_ = false;
                        break;
                    case ' ':  // Space - 暂停/继续当前播放
                        player.toggle_pause();
                        break;

                    // ═══════════════════════════════════════════════════════════
                    // 音量和速度控制
                    // 在L弹窗内+也作为音量增加，=键用于追加到播放列表
                    // ═══════════════════════════════════════════════════════════
                    case '+':  //音量增加
                        player.set_volume(player.get_state().volume + VOLUME_STEP);
                        break;
                    case '=': {  //追加当前选中项到播放列表
                        int count = 0;
                        if (!list_marked_indices.empty()) {
                            // 有标记项，追加所有标记项
                            for (int idx : list_marked_indices) {
                                if (idx >= 0 && idx < static_cast<int>(saved_playlist.size())) {
                                    // 复制已有项（追加到末尾）
                                    SavedPlaylistItem new_item = saved_playlist[idx];
                                    new_item.id = 0;  // 清除ID以便作为新项保存
                                    new_item.sort_order = static_cast<int>(saved_playlist.size());
                                    DatabaseManager::instance().save_playlist_item(new_item);
                                    saved_playlist.push_back(new_item);
                                    count++;
                                }
                            }
                            list_marked_indices.clear();
                            list_visual_mode_ = false;
                        } else if (list_selected_idx >= 0 && list_selected_idx < static_cast<int>(saved_playlist.size())) {
                            // 无标记项，追加当前选中项
                            SavedPlaylistItem new_item = saved_playlist[list_selected_idx];
                            new_item.id = 0;
                            new_item.sort_order = static_cast<int>(saved_playlist.size());
                            DatabaseManager::instance().save_playlist_item(new_item);
                            saved_playlist.push_back(new_item);
                            count = 1;
                        }
                        if (count > 0) {
                            EVENT_LOG(fmt::format("＝ Added {} items to L-list", count));
                        }
                        break;
                    }
                    case '-':
                        player.set_volume(player.get_state().volume - 5);
                        break;
                    case '[':
                        player.adjust_speed(false);
                        break;
                    case ']':
                        player.adjust_speed(true);
                        break;
                    case '\\':
                        player.set_speed(1.0);
                        break;

                    // ═══════════════════════════════════════════════════════════
                    // 退出
                    // ═══════════════════════════════════════════════════════════
                    case 'o':  // 排序L列表
                        toggle_saved_playlist_sort();
                        break;
                    case 'L':  // 退出List模式
                    case 27:   // ESC - 退出List模式
                        in_list_mode_ = false;
                        list_visual_mode_ = false;
                        EVENT_LOG("Exit List Mode");
                        break;
                    case '?':
                        ui.show_help(player.get_state());
                        break;
                    case 'q':
                    case 'Q':
                        if (ui.confirm_box("Quit PODRADIO?")) {
                            running = false;
                        }
                        break;
                }
                return;  // List模式下不处理其他按键
            }

            // V2.39-FF: 帮助显示直接处理，不使用标志
            if (visual_mode_) {
                if (ch == 'j') nav_down();
                else if (ch == 'k') nav_up();
                else if (ch == 'v') confirm_visual_selection();
                else if (ch == 'V') { visual_mode_ = false; clear_all_marks(); }  // V2.39: 大写V取消全部
                else if (ch == 27) { visual_mode_ = false; EVENT_LOG("Visual cancelled"); }
                return;
            }
            
            switch (ch) {
                case 'q':   //退出需要确认
                case 'Q':
                case 27:    // ESC键
                    if (ui.confirm_box("Quit PODRADIO?")) {
                        running = false;
                    }
                    break;
                case '?': ui.show_help(player.get_state()); break;  // V2.39-FF: 直接显示帮助弹窗
                case 'R': mode = AppMode::RADIO; current_root = radio_root; reset_search(); selected_idx = 0; break;
                case 'P': mode = AppMode::PODCAST; current_root = podcast_root; reset_search(); selected_idx = 0; break;
                case 'F': mode = AppMode::FAVOURITE; current_root = fav_root; reset_search(); selected_idx = 0; break;
                case 'H': mode = AppMode::HISTORY; current_root = history_root; reset_search(); selected_idx = 0; break;  //History模式
                case 'O': {  //ONLINE模式
                    mode = AppMode::ONLINE; 
                    current_root = OnlineState::instance().online_root; 
                    //加载搜索历史
                    OnlineState::instance().load_search_history();
                    reset_search(); 
                    selected_idx = 0;
                    EVENT_LOG("Switched to ONLINE mode - press '/' to search");
                    break;
                }
                case 'M': {
                    //模式循环现在是5个模式
                    int m = ((int)mode + 1) % 5;
                    mode = (AppMode)m;
                    current_root = (mode == AppMode::RADIO) ? radio_root : 
                                   (mode == AppMode::PODCAST) ? podcast_root : 
                                   (mode == AppMode::FAVOURITE) ? fav_root : 
                                   (mode == AppMode::HISTORY) ? history_root : OnlineState::instance().online_root;
                    reset_search();
                    selected_idx = 0;
                    break;
                }
                case 'B': {  //Online模式切换地区
                    if (mode == AppMode::ONLINE) {
                        std::string new_region = OnlineState::instance().get_next_region();
                        EVENT_LOG(fmt::format("Search region: {}", ITunesSearch::get_region_name(new_region)));
                    }
                    break;
                }
                case 'r': reset_search(); refresh_node(); break;
                case 'C': clear_playlist(); break;  //清除播放列表
                case 'k': nav_up(); break;
                case 'j': nav_down(); break;
                case 'l':
                case '\n': {  // Enter/l: 多选时追加到播放列表，否则正常进入
                    if (marked_count > 0) {
                        //有标记项时，追加到播放列表
                        int count = 0;
                        for (auto& item : display_list) {
                            if (item.node && item.node->marked) {
                                if (item.node->type == NodeType::PODCAST_EPISODE ||
                                    item.node->type == NodeType::RADIO_STREAM) {
                                    add_to_saved_playlist(item.node);
                                    count++;
                                }
                            }
                        }
                        if (count > 0) {
                            EVENT_LOG(fmt::format("＋ Added {} items to playlist", count));
                        }
                    } else {
                        enter_node(marked_count);
                    }
                    break;
                }
                case 'h': go_back(); break;
                case ' ': player.toggle_pause(); break;
                case '+':  //音量增加
                    player.set_volume(player.get_state().volume + VOLUME_STEP);
                    break;
                case '=': {  //将选择对象加入L列表
                    int count = 0;
                    if (marked_count > 0) {
                        // 有标记项，追加所有标记项
                        for (auto& item : display_list) {
                            if (item.node && item.node->marked) {
                                if (item.node->type == NodeType::PODCAST_EPISODE ||
                                    item.node->type == NodeType::RADIO_STREAM) {
                                    add_to_saved_playlist(item.node);
                                    count++;
                                }
                            }
                        }
                    } else if (selected_idx >= 0 && selected_idx < (int)display_list.size()) {
                        // 无标记项，追加当前选中项
                        auto node = display_list[selected_idx].node;
                        if (node && (node->type == NodeType::PODCAST_EPISODE ||
                                     node->type == NodeType::RADIO_STREAM)) {
                            add_to_saved_playlist(node);
                            count = 1;
                        }
                    }
                    if (count > 0) {
                        EVENT_LOG(fmt::format("＝ Added {} items to L-list", count));
                    }
                    break;
                }
                case '-': player.set_volume(player.get_state().volume - VOLUME_STEP); break;
                case ']': player.adjust_speed(true); break;
                case '[': player.adjust_speed(false); break;
                case KEY_BACKSPACE:
                case '\\': player.reset_speed(); break;  //反斜杠键也支持重置速度
                case 'g': nav_top(); break;
                case 'G': nav_bottom(); break;
                case KEY_PPAGE: nav_page_up(); break;
                case KEY_NPAGE: nav_page_down(); break;
                case KEY_RESIZE:  //终端大小变化事件处理
                    // ncurses会自动更新LINES和COLS，这里只需要触发重绘
                    // 实际的resize处理在主循环中完成
                    break;
                case 'a': {
                    //支持ONLINE/FAVOURITE模式多选订阅
                    if (mode == AppMode::PODCAST) {
                        add_feed();
                    } else if (mode == AppMode::ONLINE) {
                        if (marked_count > 0) {
                            // 多选批量订阅
                            subscribe_online_podcasts_batch(marked_count);
                        } else {
                            // 单选订阅
                            subscribe_online_podcast();
                        }
                    } else if (mode == AppMode::FAVOURITE) {
                        if (marked_count > 0) {
                            // 多选批量订阅收藏项
                            subscribe_favourites_batch(marked_count);
                        } else {
                            // 单选订阅当前收藏项
                            subscribe_favourite_single();
                        }
                    }
                    break;
                }
                case 'e': edit_node(); break;  // V2.39-FF: 编辑节点标题和URL
                case 'f': {
                    //支持多选收藏
                    if (marked_count > 0) {
                        add_favourites_batch(marked_count);
                    } else {
                        add_favourite();
                    }
                    break;
                }
                case 'd': delete_node(marked_count); break;
                case 'D': download_node(marked_count); break;
                case 'm': toggle_mark(); break;
                case 'v': visual_mode_ = true; visual_start_ = selected_idx; break;
                case 'V': clear_all_marks(); break;  // V2.39: 大写V取消全部选择
                case 'T': ui.toggle_tree_lines(); break;
                case 'S': ui.toggle_scroll_mode(); break;
                case 'U': {  // V0.05B9n3e5g3h: 切换图标风格（ASCII/Emoji）
                    IconManager::toggle_style();
                    EVENT_LOG(fmt::format("Icon style: {}", IconManager::get_style_name()));
                    break;
                }
                case 'L':  //List模式切换
                    if (in_list_mode_) {
                        in_list_mode_ = false;
                        EVENT_LOG("Exit List Mode");
                    } else {
                        load_saved_playlist();
                        in_list_mode_ = true;
                        //自动定位到当前播放的节目
                        auto play_state = player.get_state();
                        if (play_state.has_media && !play_state.current_url.empty()) {
                            // 在saved_playlist中查找当前播放的URL
                            for (size_t i = 0; i < saved_playlist.size(); ++i) {
                                if (saved_playlist[i].url == play_state.current_url) {
                                    list_selected_idx = static_cast<int>(i);
                                    EVENT_LOG(fmt::format("List: cursor at playing item #{}", i + 1));
                                    break;
                                }
                            }
                        } else {
                            list_selected_idx = 0;
                        }
                        EVENT_LOG("Enter List Mode");
                    }
                    break;
                case 12:  // Ctrl+L - 切换深色/浅色主题
                    ui.toggle_theme();
                    break;
                case 'o': toggle_sort_order(); break;     //切换排序顺序
                case '/': 
                    if (mode == AppMode::ONLINE) {
                        perform_online_search();  //Online模式搜索
                    } else if (mode == AppMode::FAVOURITE) {
                        //FAVOURITE模式下检测是否在online_root LINK节点下
                        bool under_online_link = false;
                        if (selected_idx < (int)display_list.size()) {
                            auto node = display_list[selected_idx].node;
                            // 检查当前节点或其父节点是否是online_root LINK
                            for (auto& f : fav_root->children) {
                                if (f->is_link && f->url == "online_root") {
                                    // 检查当前节点是否是f或f的子节点
                                    if (f.get() == node.get()) {
                                        under_online_link = true;
                                        break;
                                    }
                                    for (auto& child : f->children) {
                                        if (child.get() == node.get()) {
                                            under_online_link = true;
                                            break;
                                        }
                                    }
                                    if (under_online_link) break;
                                }
                            }
                        }
                        if (under_online_link) {
                            // 在online_root LINK下，执行在线搜索并同步
                            perform_online_search_from_favourite();
                        } else {
                            perform_search(); 
                        }
                    } else {
                        perform_search(); 
                    }
                    break;
                case 'J': jump_search(1); break;
                case 'K': jump_search(-1); break;
            }
        }
        
        //Online模式搜索
        //搜索历史节点化管理
        //支持ESC取消搜索
        void perform_online_search() {
            std::string query = ui.input_box("Search iTunes Podcasts");
            //检查用户是否取消
            if (UI::is_input_cancelled(query)) {
                EVENT_LOG("Search cancelled");
                return;
            }
            if (query.empty()) return;
            
            OnlineState::instance().last_query = query;
            EVENT_LOG(fmt::format("Searching: '{}' in {}", query, 
                ITunesSearch::get_region_name(OnlineState::instance().current_region)));
            
            // 执行搜索
            auto results = ITunesSearch::instance().search(query, OnlineState::instance().current_region);
            
            //添加或更新搜索节点
            OnlineState::instance().add_or_update_search_node(
                query, OnlineState::instance().current_region, results);
            
            selected_idx = 0;
            view_start = 0;
            
            EVENT_LOG(fmt::format("Found {} podcasts", results.size()));
        }
        
        //从FAVOURITE模式的online_root LINK节点执行在线搜索
        // 新增的搜索会同步到ONLINE的online_root
        void perform_online_search_from_favourite() {
            std::string query = ui.input_box("Search iTunes Podcasts (sync to ONLINE)");
            if (UI::is_input_cancelled(query)) {
                EVENT_LOG("Search cancelled");
                return;
            }
            if (query.empty()) return;
            
            OnlineState::instance().last_query = query;
            EVENT_LOG(fmt::format("Searching: '{}' in {}", query, 
                ITunesSearch::get_region_name(OnlineState::instance().current_region)));
            
            // 执行搜索
            auto results = ITunesSearch::instance().search(query, OnlineState::instance().current_region);
            
            //添加或更新搜索节点到ONLINE的online_root
            OnlineState::instance().add_or_update_search_node(
                query, OnlineState::instance().current_region, results);
            
            //清空FAVOURITE中所有online_root LINK节点的children
            // 下次展开时会重新从online_root同步
            for (auto& f : fav_root->children) {
                if (f->is_link && f->url == "online_root") {
                    f->children_loaded = false;
                    f->children.clear();
                }
            }
            
            // 展开并刷新当前LINK节点
            if (selected_idx < (int)display_list.size()) {
                auto node = display_list[selected_idx].node;
                // 找到包含当前节点的LINK
                for (auto& f : fav_root->children) {
                    if (f->is_link && f->url == "online_root") {
                        bool is_under = (f.get() == node.get());
                        if (!is_under) {
                            for (auto& child : f->children) {
                                if (child.get() == node.get()) {
                                    is_under = true;
                                    break;
                                }
                            }
                        }
                        if (is_under) {
                            // 从online_root同步children
                            {
                                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                                f->children.clear();
                                for (auto& child : OnlineState::instance().online_root->children) {
                                    child->parent = f;  //更新父节点指针
                                    f->children.push_back(child);
                                }
                            }
                            f->children_loaded = true;
                            f->expanded = true;
                            break;
                        }
                    }
                }
            }
            
            // 重新构建display_list
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                display_list.clear();
                flatten(current_root, 0, true, -1);
            }
            
            selected_idx = 0;
            view_start = 0;
            
            EVENT_LOG(fmt::format("Found {} podcasts (synced to ONLINE)", results.size()));
        }
        
        //从缓存加载搜索历史节点的子节点
        void load_search_history_children(TreeNodePtr node) {
            if (!node || node->url.find("search:") != 0) return;
            
            // 解析URL格式: search:region:query
            std::string search_id = node->url.substr(7);  // 去掉 "search:"
            size_t colon_pos = search_id.find(':');
            if (colon_pos == std::string::npos) return;
            
            std::string region = search_id.substr(0, colon_pos);
            std::string query = search_id.substr(colon_pos + 1);
            
            // 从数据库加载缓存
            std::string cached = DatabaseManager::instance().load_search_cache(query, region);
            if (cached.empty()) {
                EVENT_LOG(fmt::format("[Online] No cache for '{}'", query));
                return;
            }
            
            // 解析JSON并构建子节点
            try {
                json j = json::parse(cached);
                if (j.contains("results") && j["results"].is_array()) {
                    node->children.clear();
                    for (const auto& item : j["results"]) {
                        auto child = ITunesSearch::instance().parse_result(item);
                        if (child) {
                            // 检查是否已缓存
                            child->is_db_cached = DatabaseManager::instance().is_podcast_cached(child->url);
                            child->parent = node;  //设置父节点指针
                            node->children.push_back(child);
                        }
                    }
                    node->children_loaded = true;
                    EVENT_LOG(fmt::format("[Online] Loaded {} cached results for '{}'", 
                        node->children.size(), query));
                }
            } catch (const std::exception& e) {
                LOG(fmt::format("[Online] Parse error: {}", e.what()));
            }
        }
        
        //从数据库缓存加载收藏引用的子节点（引用模式）
        // V0.05B9n3e5g3d: 扩展支持多种节点类型
        // 对于ONLINE/RADIO/PODCAST/HISTORY模式的收藏，展开时从数据库缓存加载子节点
        // 这样收藏就是"软链接"，始终指向最新数据
        bool load_favourite_children_from_cache(TreeNodePtr node) {
            if (!node || node->url.empty()) return false;
            
            std::string url = node->url;
            URLType url_type = URLClassifier::classify(url);
            
            // ═══════════════════════════════════════════════════════════════════════════
            // V0.05B9n3e5g3d: 支持多种URL类型
            // 包括：播客Feed、YouTube频道、OPML目录、电台流等
            // ═══════════════════════════════════════════════════════════════════════════
            
            // 优先从episode_cache加载（适用于播客Feed、YouTube频道等）
            if (url_type == URLType::RSS_PODCAST || url_type == URLType::YOUTUBE_RSS || 
                url_type == URLType::YOUTUBE_CHANNEL || url_type == URLType::APPLE_PODCAST ||
                url_type == URLType::YOUTUBE_PLAYLIST) {
                
                // 检查podcast_cache是否存在
                if (!DatabaseManager::instance().is_podcast_cached(url)) {
                    EVENT_LOG(fmt::format("[Favourite] Podcast cache not found: {}", url));
                    return false;  // 返回false让调用者尝试在线解析
                }
                
                // 从episode_cache加载节目列表
                auto episodes = DatabaseManager::instance().load_episodes_from_cache(url);
                if (episodes.empty()) {
                    EVENT_LOG(fmt::format("[Favourite] No episodes cached for: {}", url));
                    return false;
                }
                
                // 构建子节点
                node->children.clear();
                for (const auto& [ep_url, ep_title, ep_duration, ep_data] : episodes) {
                    auto child = std::make_shared<TreeNode>();
                    child->title = ep_title.empty() ? "Untitled" : ep_title;
                    child->url = ep_url;
                    child->type = NodeType::PODCAST_EPISODE;
                    child->duration = ep_duration;
                    child->children_loaded = true;
                    child->parent = node;  //设置父节点指针
                    
                    // 解析data_json获取更多信息
                    if (!ep_data.empty()) {
                        try {
                            json j = json::parse(ep_data);
                            child->is_youtube = j.value("is_youtube", false);
                        } catch (...) {}
                    }
                    
                    node->children.push_back(child);
                }
                
                node->children_loaded = true;
                node->parse_failed = false;
                EVENT_LOG(fmt::format("[Favourite] Loaded {} episodes from cache", node->children.size()));
                return true;
            }
            
            // OPML/电台目录类型：检查是否有缓存的子节点
            if (url_type == URLType::OPML || 
                url.find(".opml") != std::string::npos ||
                url.find("Browse.ashx") != std::string::npos ||
                url.find("Tune.ashx") != std::string::npos) {
                
                // OPML节点尝试从episode_cache加载（某些OPML可能已缓存）
                if (DatabaseManager::instance().is_episode_cached(url)) {
                    auto episodes = DatabaseManager::instance().load_episodes_from_cache(url);
                    if (!episodes.empty()) {
                        node->children.clear();
                        for (const auto& [ep_url, ep_title, ep_duration, ep_data] : episodes) {
                            auto child = std::make_shared<TreeNode>();
                            child->title = ep_title.empty() ? "Untitled" : ep_title;
                            child->url = ep_url;
                            // OPML子节点可能是流或节目
                            URLType child_url_type = URLClassifier::classify(ep_url);
                            child->type = (child_url_type == URLType::RADIO_STREAM || 
                                          child_url_type == URLType::VIDEO_FILE) ? 
                                          NodeType::RADIO_STREAM : NodeType::PODCAST_EPISODE;
                            child->duration = ep_duration;
                            child->children_loaded = true;
                            child->parent = node;
                            
                            if (!ep_data.empty()) {
                                try {
                                    json j = json::parse(ep_data);
                                    child->is_youtube = j.value("is_youtube", false);
                                } catch (...) {}
                            }
                            
                            node->children.push_back(child);
                        }
                        
                        node->children_loaded = true;
                        node->parse_failed = false;
                        EVENT_LOG(fmt::format("[Favourite] Loaded {} OPML items from cache", node->children.size()));
                        return true;
                    }
                }
                
                // OPML缓存未命中，返回false让调用者在线解析
                EVENT_LOG(fmt::format("[Favourite] OPML cache not found: {}", url));
                return false;
            }
            
            // 其他类型：尝试从episode_cache加载
            if (DatabaseManager::instance().is_episode_cached(url)) {
                auto episodes = DatabaseManager::instance().load_episodes_from_cache(url);
                if (!episodes.empty()) {
                    node->children.clear();
                    for (const auto& [ep_url, ep_title, ep_duration, ep_data] : episodes) {
                        auto child = std::make_shared<TreeNode>();
                        child->title = ep_title.empty() ? "Untitled" : ep_title;
                        child->url = ep_url;
                        child->type = NodeType::PODCAST_EPISODE;
                        child->duration = ep_duration;
                        child->children_loaded = true;
                        child->parent = node;
                        
                        if (!ep_data.empty()) {
                            try {
                                json j = json::parse(ep_data);
                                child->is_youtube = j.value("is_youtube", false);
                            } catch (...) {}
                        }
                        
                        node->children.push_back(child);
                    }
                    
                    node->children_loaded = true;
                    node->parse_failed = false;
                    EVENT_LOG(fmt::format("[Favourite] Loaded {} items from cache (generic)", node->children.size()));
                    return true;
                }
            }
            
            return false;
        }
        
        //订阅Online搜索结果中的播客
        void subscribe_online_podcast() {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            if (!node || node->url.empty()) return;
            
            // 检查是否已存在
            for (const auto& child : podcast_root->children) {
                if (child->url == node->url) {
                    EVENT_LOG("Already subscribed");
                    return;
                }
            }
            
            // 添加到PODCAST列表
            auto new_node = std::make_shared<TreeNode>();
            new_node->title = node->title;
            new_node->url = node->url;
            new_node->type = NodeType::PODCAST_FEED;
            new_node->subtext = node->subtext;
            
            podcast_root->children.insert(podcast_root->children.begin(), new_node);
            Persistence::save_cache(radio_root, podcast_root);
            Persistence::save_data(podcast_root->children, fav_root->children);
            
            EVENT_LOG(fmt::format("Subscribed: {}", node->title));
        }
        
        //ONLINE模式多选批量订阅
        void subscribe_online_podcasts_batch(int marked_count) {
            (void)marked_count; // 保留参数供未来使用
            std::vector<TreeNodePtr> to_subscribe;
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                collect_marked(OnlineState::instance().online_root, to_subscribe);
            }
            
            if (to_subscribe.empty()) {
                EVENT_LOG("No items to subscribe");
                return;
            }
            
            // 过滤出可订阅的节点（PODCAST_FEED类型）
            std::vector<TreeNodePtr> feed_nodes;
            for (auto& n : to_subscribe) {
                if (n->type == NodeType::PODCAST_FEED && !n->url.empty()) {
                    // 检查是否已订阅
                    bool already_subscribed = false;
                    for (const auto& child : podcast_root->children) {
                        if (child->url == n->url) {
                            already_subscribed = true;
                            break;
                        }
                    }
                    if (!already_subscribed) {
                        feed_nodes.push_back(n);
                    }
                }
            }
            
            if (feed_nodes.empty()) {
                EVENT_LOG("All selected items already subscribed");
                return;
            }
            
            // 批量订阅
            int count = 0;
            for (auto& n : feed_nodes) {
                auto new_node = std::make_shared<TreeNode>();
                new_node->title = n->title;
                new_node->url = n->url;
                new_node->type = NodeType::PODCAST_FEED;
                new_node->subtext = n->subtext;
                
                podcast_root->children.insert(podcast_root->children.begin(), new_node);
                count++;
            }
            
            // 清除标记
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                clear_marks(OnlineState::instance().online_root);
            }
            
            Persistence::save_cache(radio_root, podcast_root);
            Persistence::save_data(podcast_root->children, fav_root->children);
            
            EVENT_LOG(fmt::format("Subscribed {} podcasts", count));
        }
        
        //FAVOURITE模式单选订阅
        void subscribe_favourite_single() {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            if (!node || node->url.empty()) return;
            
            // 检查是否已订阅
            for (const auto& child : podcast_root->children) {
                if (child->url == node->url) {
                    EVENT_LOG("Already subscribed");
                    return;
                }
            }
            
            // 添加到PODCAST列表
            auto new_node = std::make_shared<TreeNode>();
            new_node->title = node->title;
            new_node->url = node->url;
            new_node->type = NodeType::PODCAST_FEED;
            new_node->is_youtube = node->is_youtube;
            
            podcast_root->children.insert(podcast_root->children.begin(), new_node);
            Persistence::save_cache(radio_root, podcast_root);
            Persistence::save_data(podcast_root->children, fav_root->children);
            
            EVENT_LOG(fmt::format("Subscribed: {}", node->title));
        }
        
        //FAVOURITE模式多选批量订阅
        void subscribe_favourites_batch(int marked_count) {
            (void)marked_count; // 保留参数供未来使用
            std::vector<TreeNodePtr> to_subscribe;
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                collect_marked(fav_root, to_subscribe);
            }
            
            if (to_subscribe.empty()) {
                EVENT_LOG("No items to subscribe");
                return;
            }
            
            // 过滤出可订阅的节点
            std::vector<TreeNodePtr> feed_nodes;
            for (auto& n : to_subscribe) {
                // 支持PODCAST_FEED和有URL的节点
                if (!n->url.empty() && n->url != "online_root") {
                    // 检查是否已订阅
                    bool already_subscribed = false;
                    for (const auto& child : podcast_root->children) {
                        if (child->url == n->url) {
                            already_subscribed = true;
                            break;
                        }
                    }
                    if (!already_subscribed) {
                        feed_nodes.push_back(n);
                    }
                }
            }
            
            if (feed_nodes.empty()) {
                EVENT_LOG("All selected items already subscribed");
                return;
            }
            
            // 批量订阅
            int count = 0;
            for (auto& n : feed_nodes) {
                auto new_node = std::make_shared<TreeNode>();
                new_node->title = n->title;
                new_node->url = n->url;
                new_node->type = NodeType::PODCAST_FEED;
                new_node->is_youtube = n->is_youtube;
                
                podcast_root->children.insert(podcast_root->children.begin(), new_node);
                count++;
            }
            
            // 清除标记
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                clear_marks(fav_root);
            }
            
            Persistence::save_cache(radio_root, podcast_root);
            Persistence::save_data(podcast_root->children, fav_root->children);
            
            EVENT_LOG(fmt::format("Subscribed {} podcasts from favourites", count));
        }
        
        //多选批量收藏
        //ONLINE模式批量收藏也转为LINK
        void add_favourites_batch(int marked_count) {
            //ONLINE模式特殊处理
            if (mode == AppMode::ONLINE) {
                // 检查是否已存在online_root收藏
                for (auto& f : fav_root->children) {
                    if (f->url == "online_root") {
                        EVENT_LOG("★ Online Search already in favourites");
                        // 清除标记
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        clear_marks(current_root);
                        return;
                    }
                }
                
                // ONLINE模式批量收藏也只创建一个LINK
                auto fn = std::make_shared<TreeNode>();
                fn->title = "Online Search";
                fn->url = "online_root";
                fn->type = NodeType::FOLDER;
                fn->children_loaded = false;
                
                std::string source_type = "online_root_reference";
                json ref_json;
                ref_json["batch_mode"] = true;
                ref_json["marked_count"] = marked_count;
                std::string data_json = ref_json.dump();
                
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    fav_root->children.insert(fav_root->children.begin(), fn);
                    clear_marks(current_root);
                }
                
                DatabaseManager::instance().save_favourite(
                    fn->title, fn->url, (int)fn->type,
                    fn->is_youtube, fn->channel_name, source_type, data_json
                );
                
                EVENT_LOG("★ Favourite: Online Search (LINK to online_root)");
                return;
            }
            
            // 非ONLINE模式保持原有批量收藏逻辑
            std::vector<TreeNodePtr> to_fav;
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                collect_marked(current_root, to_fav);
            }
            
            if (to_fav.empty()) {
                EVENT_LOG("No items to favourite");
                return;
            }
            
            // 过滤出可收藏的节点
            std::vector<TreeNodePtr> fav_nodes;
            std::set<std::string> existing_urls;
            for (const auto& f : fav_root->children) {
                if (!f->url.empty()) existing_urls.insert(f->url);
            }
            
            for (auto& n : to_fav) {
                // 支持所有节点类型的收藏
                if (n->type == NodeType::PODCAST_FEED || 
                    n->type == NodeType::FOLDER ||
                    n->type == NodeType::RADIO_STREAM ||
                    n->type == NodeType::PODCAST_EPISODE) {
                    // 检查是否已收藏（URL匹配或标题匹配）
                    bool already_fav = false;
                    if (!n->url.empty()) {
                        already_fav = existing_urls.count(n->url) > 0;
                    } else {
                        for (const auto& f : fav_root->children) {
                            if (f->title == n->title && f->type == n->type) {
                                already_fav = true;
                                break;
                            }
                        }
                    }
                    if (!already_fav) {
                        fav_nodes.push_back(n);
                    }
                }
            }
            
            if (fav_nodes.empty()) {
                EVENT_LOG("All selected items already favourited");
                return;
            }
            
            //批量收藏也使用LINK机制
            // 获取来源模式名称
            std::string source_mode_name;
            switch (mode) {
                case AppMode::RADIO: source_mode_name = "RADIO"; break;
                case AppMode::PODCAST: source_mode_name = "PODCAST"; break;
                case AppMode::ONLINE: source_mode_name = "ONLINE"; break;
                case AppMode::FAVOURITE: source_mode_name = "FAVOURITE"; break;
                case AppMode::HISTORY: source_mode_name = "HISTORY"; break;
            }
            
            // 批量收藏
            int count = 0;
            for (auto& n : fav_nodes) {
                // 创建LINK节点
                auto fn = std::make_shared<TreeNode>();
                fn->title = n->title;
                fn->url = n->url;                    // 目标URL
                fn->type = n->type;                  // 目标类型
                fn->is_link = true;                  // 标记为LINK
                fn->link_target_url = n->url;        // 持久化目标URL
                fn->source_mode = source_mode_name;  // 来源模式
                fn->linked_node = n;                 // 运行时引用
                fn->is_youtube = n->is_youtube;
                fn->channel_name = n->channel_name;
                fn->children_loaded = false;         // LINK节点延迟加载
                
                fn->parent = fav_root;
                fav_root->children.insert(fav_root->children.begin(), fn);
                
                // 保存到数据库（包含LINK信息）
                json link_info;
                link_info["is_link"] = true;
                link_info["source_mode"] = source_mode_name;
                link_info["link_target_url"] = fn->link_target_url;
                std::string data_json = link_info.dump();
                
                DatabaseManager::instance().save_favourite(
                    fn->title, fn->url, (int)fn->type,
                    fn->is_youtube, fn->channel_name, source_mode_name, data_json
                );
                
                count++;
            }
            
            // 清除标记
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                clear_marks(current_root);
            }
            
            EVENT_LOG(fmt::format("★ Added {} favourites", count));
        }
        
        void clear_all_marks() {
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            clear_marks(current_root);
            EVENT_LOG("All marks cleared");
            Persistence::save_cache(radio_root, podcast_root);
        }
        
        //增强Visual多选 - 支持所有模式的所有节点类型
        void confirm_visual_selection() {
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            int start = std::min(visual_start_, selected_idx);
            int end = std::max(visual_start_, selected_idx);
            int count = 0;
            
            for (int i = start; i <= end && i < (int)display_list.size(); ++i) {
                auto node = display_list[i].node;
                //支持所有节点类型的多选
                // RADIO_STREAM/PODCAST_EPISODE: 用于批量下载/删除
                // FOLDER/PODCAST_FEED: 用于批量删除（ONLINE/FAVOURITE模式）
                // 不标记空URL的文件夹（无实际操作意义）
                if (node->type == NodeType::RADIO_STREAM || 
                    node->type == NodeType::PODCAST_EPISODE ||
                    node->type == NodeType::FOLDER ||
                    node->type == NodeType::PODCAST_FEED) {
                    node->marked = true;
                    count++;
                }
            }
            
            visual_mode_ = false;
            visual_start_ = -1;
            EVENT_LOG(fmt::format("Marked {} items", count));
            Persistence::save_cache(radio_root, podcast_root);
        }

        void nav_up() { if (selected_idx > 0) selected_idx--; }
        void nav_down() { if (selected_idx < (int)display_list.size() - 1) selected_idx++; }
        void nav_top() { selected_idx = 0; view_start = 0; }
        void nav_bottom() { selected_idx = display_list.size() - 1; }
        void nav_page_up() { selected_idx -= PAGE_SCROLL_LINES; if (selected_idx < 0) selected_idx = 0; }
        void nav_page_down() { selected_idx += PAGE_SCROLL_LINES; if (selected_idx >= (int)display_list.size()) selected_idx = display_list.size() - 1; }
        
        void go_back() {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            
            // V2.39-FF: 改进'h'键行为
            // 如果当前节点是可展开的且已展开，先折叠
            if ((node->type == NodeType::FOLDER || node->type == NodeType::PODCAST_FEED) && node->expanded) {
                node->expanded = false;
                return;
            }
            
            // 如果节点没有展开，尝试跳转到父节点
            int parent = display_list[selected_idx].parent_idx;
            if (parent != -1) {
                selected_idx = parent;
            } else {
                // V2.39-FF: 最上级节点时，检查是否有可折叠的顶级节点
                // 尝试折叠所有顶级子节点
                bool collapsed = false;
                for (auto& child : current_root->children) {
                    if (child->expanded) {
                        child->expanded = false;
                        collapsed = true;
                    }
                }
                if (!collapsed) {
                    EVENT_LOG("Already at top level");
                }
            }
        }
        
        // ═══════════════════════════════════════════════════════════════════════════
        //重构LINK展开机制

        // 核心理念：F模式是"聚合链接汇总表"，不存储数据，只引用真实对象

        // LINK节点展开策略：
        // 1. 找到真实节点(target) - 运行时引用 → 内存查找 → 创建新节点
        // 2. 如果真实节点未加载，在真实节点上触发解析
        // 3. 解析完成后，LINK节点同步引用真实节点的children
        // 4. 数据缓存存储在真实节点下（R/P/ONLINE模式）

        //关键修复 - 解析方法选择优先级
        // 优先级：source_mode > node->type > URL类型
        // ────────────────────────────────────────────────────────────────────────
        // source_mode    │ node->type        │ 解析方法
        // ────────────────────────────────────────────────────────────────────────
        // RADIO          │ *                 │ spawn_load_radio()
        // PODCAST        │ *                 │ spawn_load_feed()
        // ONLINE         │ FOLDER            │ spawn_load_radio() (搜索结果中的文件夹)
        // ONLINE         │ PODCAST_FEED      │ spawn_load_feed()
        // ONLINE         │ * (其他)          │ spawn_load_feed()
        // ═══════════════════════════════════════════════════════════════════════════
        
        //根据source_mode字符串获取对应的root节点
        TreeNodePtr get_root_by_mode_string(const std::string& mode_str) {
            if (mode_str == "RADIO") return radio_root;
            if (mode_str == "PODCAST") return podcast_root;
            if (mode_str == "ONLINE") return OnlineState::instance().online_root;
            if (mode_str == "FAVOURITE") return fav_root;
            if (mode_str == "HISTORY") return history_root;
            return nullptr;
        }
        
        //根据source_mode和type选择解析方法
        // 返回: true=使用spawn_load_radio, false=使用spawn_load_feed
        bool should_use_radio_loader(const std::string& source_mode, NodeType node_type, const std::string& url) {
            // 优先级1：source_mode决定主要解析器
            if (source_mode == "RADIO") {
                return true;  // RADIO模式统一使用radio解析器
            }
            if (source_mode == "PODCAST") {
                return false; // PODCAST模式统一使用feed解析器
            }
            
            // 优先级2：ONLINE模式根据node_type判断
            if (source_mode == "ONLINE") {
                // ONLINE搜索结果可能是电台文件夹或播客Feed
                if (node_type == NodeType::FOLDER) {
                    return true;  // 文件夹类型用radio解析器
                }
                return false;     // 其他类型用feed解析器
            }
            
            // 优先级3：根据URL类型判断（fallback）
            URLType url_type = URLClassifier::classify(url);
            if (url_type == URLType::OPML || 
                url.find(".opml") != std::string::npos ||
                url.find("Browse.ashx") != std::string::npos ||
                url.find("Tune.ashx") != std::string::npos) {
                return true;
            }
            
            // 优先级4：根据节点类型判断
            if (node_type == NodeType::FOLDER) {
                return true;
            }
            
            return false;
        }
        
        //同步LINK节点状态（同时检查fav_root和podcast_root）
        // 当target节点加载完成后，同步更新所有引用该节点的LINK节点
        // 这个函数在spawn_load_feed/spawn_load_radio完成后被调用
        void sync_link_node_status(TreeNodePtr target) {
            if (!target || !target->children_loaded) return;
            
            // 遍历favourite_root和podcast_root，找到所有引用该target的LINK节点
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            
            std::function<void(TreeNodePtr)> sync_children = [&](TreeNodePtr parent) {
                for (auto& child : parent->children) {
                    if (child->is_link) {
                        auto linked = child->linked_node.lock();
                        if (linked.get() == target.get()) {
                            // 找到引用该target的LINK节点，同步状态
                            child->loading = false;
                            child->children_loaded = true;
                            child->parse_failed = target->parse_failed;
                            child->error_msg = target->error_msg;
                            
                            // 同步children
                            if (!target->children.empty()) {
                                child->children.clear();
                                for (auto& tc : target->children) {
                                    tc->parent = child;
                                    child->children.push_back(tc);
                                }
                            }
                            
                            EVENT_LOG(fmt::format("[LINK] Synced status from target: {}", child->title));
                        }
                    }
                    // 递归处理子节点
                    sync_children(child);
                }
            };
            
            //同时检查fav_root和podcast_root
            // 因为YouTube频道可能直接在PODCAST模式订阅，也可能在FAVOURITE模式收藏
            sync_children(fav_root);
            sync_children(podcast_root);
        }
        
        bool expand_link_node(TreeNodePtr node) {
            if (!node || !node->is_link) return false;
            
            std::string target_url = node->link_target_url.empty() ? node->url : node->link_target_url;
            
            //确保loading状态正确初始化
            // 如果target已经加载完成，确保LINK节点的loading为false
            TreeNodePtr early_target = node->linked_node.lock();
            if (early_target && early_target->children_loaded && !early_target->children.empty()) {
                node->loading = false;
            }
            
            // ─────────────────────────────────────────────────────────────────
            // 步骤1：找到真实节点(target)
            // ─────────────────────────────────────────────────────────────────
            TreeNodePtr target = node->linked_node.lock();
            
            // 尝试从内存查找
            if (!target) {
                TreeNodePtr search_root = get_root_by_mode_string(node->source_mode);
                
                // 特殊处理：online_root
                if (target_url == "online_root") {
                    if (!OnlineState::instance().history_loaded) {
                        OnlineState::instance().load_search_history();
                    }
                    target = OnlineState::instance().online_root;
                }
                // 特殊处理：搜索记录节点
                else if (target_url.find("search:") == 0) {
                    // 搜索记录节点直接加载到LINK节点，不需要真实节点
                    load_search_history_children(node);
                    node->expanded = !node->children.empty();
                    node->loading = false;  //确保loading状态更新
                    EVENT_LOG(fmt::format("[LINK] Loaded search record: {}", target_url));
                    return !node->children.empty();
                }
                // 普通节点：在对应root中查找
                else if (search_root) {
                    if (node->source_mode == "ONLINE") {
                        // ONLINE模式需要确保历史已加载
                        if (!OnlineState::instance().history_loaded) {
                            OnlineState::instance().load_search_history();
                        }
                    }
                    target = find_node_by_url(search_root, target_url);
                }
            }
            
            // ─────────────────────────────────────────────────────────────────
            // 步骤2：如果找到真实节点，检查是否需要加载
            // ─────────────────────────────────────────────────────────────────
            if (target) {
                // 更新运行时引用
                node->linked_node = target;
                
                // 检查真实节点是否已加载children
                if (target->children_loaded && !target->children.empty()) {
                    // 真实节点已加载，直接同步children到LINK
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        node->children.clear();
                        for (auto& child : target->children) {
                            child->parent = node;
                            node->children.push_back(child);
                        }
                    }
                    node->children_loaded = true;
                    node->expanded = true;
                    node->loading = false;  //确保loading状态更新
                    EVENT_LOG(fmt::format("[LINK] Synced from target: {} items", node->children.size()));
                    return true;
                }
                
                // ─────────────────────────────────────────────────────────────────
                // 关键修复：真实节点未加载，在真实节点上触发解析
                // 数据将缓存在真实节点下，而不是LINK节点
                //使用should_use_radio_loader选择解析方法
                // ─────────────────────────────────────────────────────────────────
                EVENT_LOG(fmt::format("[LINK] Target not loaded, loading on target: {} (source_mode={}, type={})",
                    target->title, node->source_mode, (int)target->type));
                
                // 根据source_mode和type选择解析方法
                if (should_use_radio_loader(node->source_mode, target->type, target_url)) {
                    // 电台/文件夹类型：在真实节点上加载
                    spawn_load_radio(target);
                    node->loading = true;
                    EVENT_LOG(fmt::format("[LINK] Loading RADIO on target: {}", target_url));
                    return true;
                }
                
                // 播客类型：优先从数据库缓存加载到真实节点
                if (DatabaseManager::instance().is_episode_cached(target_url)) {
                    auto episodes = DatabaseManager::instance().load_episodes_from_cache(target_url);
                    if (!episodes.empty()) {
                        {
                            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                            target->children.clear();
                            for (const auto& [ep_url, ep_title, ep_duration, ep_data] : episodes) {
                                auto child = std::make_shared<TreeNode>();
                                child->title = ep_title.empty() ? "Untitled" : ep_title;
                                child->url = ep_url;
                                child->type = NodeType::PODCAST_EPISODE;
                                child->duration = ep_duration;
                                child->children_loaded = true;
                                child->parent = target;
                                if (!ep_data.empty()) {
                                    try {
                                        json j = json::parse(ep_data);
                                        child->is_youtube = j.value("is_youtube", false);
                                    } catch (...) {}
                                }
                                target->children.push_back(child);
                            }
                        }
                        target->children_loaded = true;
                        target->is_db_cached = true;
                        // 同步到LINK节点
                        {
                            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                            node->children.clear();
                            for (auto& child : target->children) {
                                child->parent = node;
                                node->children.push_back(child);
                            }
                        }
                        node->children_loaded = true;
                        node->expanded = true;
                        node->loading = false;  //确保loading状态更新
                        EVENT_LOG(fmt::format("[LINK] Loaded {} episodes from cache to target", episodes.size()));
                        return true;
                    }
                }
                
                // 缓存未命中，在真实节点上触发网络加载
                spawn_load_feed(target);
                node->loading = true;
                EVENT_LOG(fmt::format("[LINK] Loading PODCAST on target: {}", target_url));
                return true;
            }
            
            // ─────────────────────────────────────────────────────────────────
            // 步骤3：真实节点未找到，尝试在原始位置创建并加载
            // 这种情况发生在：收藏时真实节点存在，但后来被删除
            // ─────────────────────────────────────────────────────────────────
            
            // 特殊处理：online_root
            if (target_url == "online_root") {
                if (!OnlineState::instance().history_loaded) {
                    OnlineState::instance().load_search_history();
                }
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    node->children.clear();
                    for (auto& child : OnlineState::instance().online_root->children) {
                        child->parent = node;
                        node->children.push_back(child);
                    }
                }
                node->children_loaded = true;
                node->expanded = true;
                node->loading = false;  //确保loading状态更新
                node->linked_node = OnlineState::instance().online_root;
                EVENT_LOG("[LINK] Loaded from online_root");
                return true;
            }
            
            // 在原始root下创建新节点并加载
            // 这样新创建的节点就是"真实节点"，LINK引用它
            TreeNodePtr new_target = std::make_shared<TreeNode>();
            new_target->title = node->title;
            new_target->url = target_url;
            new_target->type = node->type;
            new_target->is_youtube = node->is_youtube;
            new_target->channel_name = node->channel_name;
            
            // 将新节点添加到对应的root
            TreeNodePtr target_root = get_root_by_mode_string(node->source_mode);
            if (target_root) {
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    new_target->parent = target_root;
                    target_root->children.insert(target_root->children.begin(), new_target);
                }
                // 更新LINK引用
                node->linked_node = new_target;
                
                //使用should_use_radio_loader选择解析方法
                if (should_use_radio_loader(node->source_mode, node->type, target_url)) {
                    spawn_load_radio(new_target);
                    EVENT_LOG(fmt::format("[LINK] Created new RADIO target: {}", target_url));
                } else {
                    spawn_load_feed(new_target);
                    EVENT_LOG(fmt::format("[LINK] Created new PODCAST target: {}", target_url));
                }
                node->loading = true;
                return true;
            }
            
            // 最后备选：直接在LINK节点上加载（不推荐，但保证功能可用）
            EVENT_LOG(fmt::format("[LINK] Fallback: loading directly on LINK node: {}", target_url));
            
            //使用should_use_radio_loader选择解析方法
            if (should_use_radio_loader(node->source_mode, node->type, target_url)) {
                spawn_load_radio(node);
            } else if (load_favourite_children_from_cache(node)) {
                node->expanded = true;
                return true;
            } else {
                spawn_load_feed(node);
            }
            return true;
        }
        
        //递归查找URL匹配的节点
        TreeNodePtr find_node_by_url(TreeNodePtr root, const std::string& url) {
            if (!root) return nullptr;
            if (root->url == url) return root;
            
            for (auto& child : root->children) {
                auto found = find_node_by_url(child, url);
                if (found) return found;
            }
            return nullptr;
        }
        
        void enter_node(int marked_count) {
            if (marked_count > 0) {
                std::vector<std::string> urls;
                std::vector<TreeNodePtr> items;
                bool has_video = false;
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    collect_playable_marked(current_root, items);
                    for (auto& n : items) {
                        URLType url_type = URLClassifier::classify(n->url);
                        if (n->is_youtube || URLClassifier::is_video(url_type)) {
                            has_video = true;
                        }
                    }
                }
                if (!items.empty()) {
                    playback_node = items[0];
                    
                    // 构建URL列表
                    for (auto& n : items) {
                        std::string local = CacheManager::instance().get_local_file(n->url);
                        if (!local.empty() && fs::exists(local)) {
                            urls.push_back("file://" + local);
                        } else {
                            urls.push_back(n->url);
                        }
                    }
                    
                    //保存播放列表
                    current_playlist.clear();
                    for (size_t i = 0; i < items.size(); ++i) {
                        PlaylistItem pl_item;
                        pl_item.title = items[i]->title;
                        pl_item.url = urls[i];
                        pl_item.duration = items[i]->duration;
                        URLType url_type = URLClassifier::classify(items[i]->url);
                        pl_item.is_video = items[i]->is_youtube || URLClassifier::is_video(url_type);
                        current_playlist.push_back(pl_item);
                    }
                    current_playlist_index = 0;
                    
                    player.play_list(urls, has_video);
                    EVENT_LOG(fmt::format("OPERATOR COMMAND: Play batch ({} items)", urls.size()));
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        clear_marks(current_root);
                    }
                    Persistence::save_cache(radio_root, podcast_root);
                }
                return;
            }
            
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            
            if (node->type == NodeType::FOLDER || node->type == NodeType::PODCAST_FEED) {
                if (!node->expanded) {
                    //检测是否是搜索历史节点
                    bool is_search_history_node = (node->url.find("search:") == 0);
                    
                    //修复：不仅检查children_loaded，还要检查children是否为空
                    // 如果children为空但URL不为空（可加载），则重新加载
                    bool need_load = (!node->children_loaded || node->children.empty()) && !node->url.empty();
                    
                    if (need_load) {
                        if (is_search_history_node) {
                            //从缓存加载搜索历史节点的子节点
                            load_search_history_children(node);
                            node->expanded = true;
                        } else if (mode == AppMode::RADIO) {
                            spawn_load_radio(node);
                        } else if (mode == AppMode::ONLINE) {
                            //Online模式优先从数据库缓存加载
                            if (node->type == NodeType::PODCAST_FEED && 
                                DatabaseManager::instance().is_episode_cached(node->url)) {
                                // 从数据库缓存加载节目列表
                                auto episodes = DatabaseManager::instance().load_episodes_from_cache(node->url);
                                if (!episodes.empty()) {
                                    node->children.clear();
                                    for (const auto& [ep_url, ep_title, ep_duration, ep_data] : episodes) {
                                        auto child = std::make_shared<TreeNode>();
                                        child->title = ep_title.empty() ? "Untitled" : ep_title;
                                        child->url = ep_url;
                                        child->type = NodeType::PODCAST_EPISODE;
                                        child->duration = ep_duration;
                                        child->children_loaded = true;
                                        child->is_db_cached = true;
                                        child->parent = node;  //设置父节点指针
                                        // 解析data_json获取更多信息
                                        if (!ep_data.empty()) {
                                            try {
                                                json j = json::parse(ep_data);
                                                child->is_youtube = j.value("is_youtube", false);
                                            } catch (...) {}
                                        }
                                        node->children.push_back(child);
                                    }
                                    node->children_loaded = true;
                                    node->expanded = true;
                                    EVENT_LOG(fmt::format("[Online] Loaded {} episodes from cache", node->children.size()));
                                } else {
                                    spawn_load_feed(node);
                                }
                            } else {
                                spawn_load_feed(node);
                            }
                        } else if (mode == AppMode::FAVOURITE) {
                            // ═══════════════════════════════════════════════════════════
                            //统一LINK展开机制
                            // 所有收藏节点都是LINK，使用统一的展开函数
                            // ═══════════════════════════════════════════════════════════
                            if (node->is_link) {
                                expand_link_node(node);
                            } else if (node->url.find("search:") == 0) {
                                // 搜索记录节点：从数据库缓存加载
                                load_search_history_children(node);
                                node->expanded = !node->children.empty();
                            } else if (node->type == NodeType::PODCAST_FEED) {
                                // 播客Feed：优先从数据库缓存加载
                                if (load_favourite_children_from_cache(node)) {
                                    node->expanded = true;
                                } else {
                                    spawn_load_feed(node);
                                }
                            } else if (node->type == NodeType::FOLDER) {
                                // 文件夹：优先从本地数据加载，再在线解析
                                URLType url_type = URLClassifier::classify(node->url);

                                // 先尝试从episode_cache加载（适用于播客目录）
                                if (DatabaseManager::instance().is_episode_cached(node->url)) {
                                    auto episodes = DatabaseManager::instance().load_episodes_from_cache(node->url);
                                    if (!episodes.empty()) {
                                        node->children.clear();
                                        for (const auto& [ep_url, ep_title, ep_duration, ep_data] : episodes) {
                                            auto child = std::make_shared<TreeNode>();
                                            child->title = ep_title.empty() ? "Untitled" : ep_title;
                                            child->url = ep_url;
                                            child->type = NodeType::PODCAST_EPISODE;
                                            child->duration = ep_duration;
                                            child->children_loaded = true;
                                            child->is_db_cached = true;
                                            child->parent = node;
                                            if (!ep_data.empty()) {
                                                try {
                                                    json j = json::parse(ep_data);
                                                    child->is_youtube = j.value("is_youtube", false);
                                                } catch (...) {}
                                            }
                                            node->children.push_back(child);
                                        }
                                        node->children_loaded = true;
                                        node->expanded = true;
                                        EVENT_LOG(fmt::format("[Favourite] Loaded {} items from cache", node->children.size()));
                                    } else {
                                        // 缓存为空，在线解析
                                        if (url_type == URLType::OPML ||
                                            node->url.find(".opml") != std::string::npos ||
                                            node->url.find("Browse.ashx") != std::string::npos) {
                                            spawn_load_radio(node);
                                        } else {
                                            spawn_load_feed(node);
                                        }
                                    }
                                } else {
                                    // 无缓存，在线解析
                                    if (url_type == URLType::OPML ||
                                        node->url.find(".opml") != std::string::npos ||
                                        node->url.find("Browse.ashx") != std::string::npos) {
                                        spawn_load_radio(node);
                                    } else {
                                        spawn_load_feed(node);
                                    }
                                }
                            } else {
                                // 其他类型：优先尝试从缓存加载
                                if (load_favourite_children_from_cache(node)) {
                                    node->expanded = true;
                                } else {
                                    spawn_load_feed(node);
                                }
                            }
                        } else if (mode == AppMode::PODCAST) {
                            // ═══════════════════════════════════════════════════════════
                            //PODCAST模式优先从数据库缓存加载
                            // ═══════════════════════════════════════════════════════════
                            if (node->type == NodeType::PODCAST_FEED && 
                                DatabaseManager::instance().is_episode_cached(node->url)) {
                                // 从数据库缓存加载节目列表
                                auto episodes = DatabaseManager::instance().load_episodes_from_cache(node->url);
                                if (!episodes.empty()) {
                                    node->children.clear();
                                    for (const auto& [ep_url, ep_title, ep_duration, ep_data] : episodes) {
                                        auto child = std::make_shared<TreeNode>();
                                        child->title = ep_title.empty() ? "Untitled" : ep_title;
                                        child->url = ep_url;
                                        child->type = NodeType::PODCAST_EPISODE;
                                        child->duration = ep_duration;
                                        child->children_loaded = true;
                                        child->is_db_cached = true;
                                        child->parent = node;
                                        if (!ep_data.empty()) {
                                            try {
                                                json j = json::parse(ep_data);
                                                child->is_youtube = j.value("is_youtube", false);
                                            } catch (...) {}
                                        }
                                        node->children.push_back(child);
                                    }
                                    node->children_loaded = true;
                                    node->expanded = true;
                                    EVENT_LOG(fmt::format("[Podcast] Loaded {} episodes from cache", node->children.size()));
                                } else {
                                    spawn_load_feed(node);
                                }
                            } else {
                                spawn_load_feed(node);
                            }
                        } else {
                            // 其他模式从网络加载
                            spawn_load_feed(node);
                        }
                    } else {
                        node->expanded = true;
                    }
                } else {
                    //LINK节点折叠时重置状态，下次展开重新加载
                    if (node->is_link) {
                        node->children_loaded = false;
                        node->children.clear();
                    }
                    node->expanded = false;
                }
            } else if (!node->url.empty()) {
                // ═══════════════════════════════════════════════════════════════════
                // V0.05B9n3e5g3n: 播放状态决策表
                //
                // 状态          │ 操作对象      │ 行为
                // ──────────────┼───────────────┼─────────────────────────────────
                // PLAYING       │ 同一节目      │ 无操作（已在播放）
                // PLAYING       │ 不同节目      │ 仅添加到列表末尾
                // PAUSED/STOP   │ 同一节目      │ 继续播放（resume）
                // PAUSED/STOP   │ 不同节目      │ 填充siblings + 播放新节目（覆盖）
                // 未播放        │ 任意节目      │ 填充siblings + 播放
                // ═══════════════════════════════════════════════════════════════════
                
                auto play_state = player.get_state();
                bool has_media = play_state.has_media;
                bool is_playing = has_media && !play_state.paused;  // 真正在播放
                bool is_paused = has_media && play_state.paused;     // 暂停状态
                bool is_same_as_playing = has_media && play_state.current_url == node->url;
                
                // PLAYING状态 + 不同节目：仅添加到列表
                if (is_playing && !is_same_as_playing) {
                    int existing_idx = find_url_in_saved_playlist(node->url);
                    if (existing_idx < 0) {
                        add_to_saved_playlist(node);
                        EVENT_LOG(fmt::format("➕ Added to playlist (while playing): {}", node->title));
                    } else {
                        EVENT_LOG(fmt::format("Already in playlist: {}", node->title));
                    }
                    return;  // 不打断当前播放
                }
                
                // PLAYING状态 + 同一节目：不做任何操作
                if (is_playing && is_same_as_playing) {
                    EVENT_LOG(fmt::format("Already playing: {}", node->title));
                    return;
                }
                
                // PAUSED状态 + 同一节目：继续播放（resume）
                if (is_paused && is_same_as_playing) {
                    player.toggle_pause();  // 取消暂停，继续播放
                    EVENT_LOG(fmt::format("Resume playing: {}", node->title));
                    return;
                }
                
                // PAUSED状态 + 不同节目：走填充路径（覆盖当前列表）
                // 即：清空列表 → 填充新节目的siblings → 播放新节目
                
                // ═══════════════════════════════════════════════════════════════════
                // 以下为未播放/PAUSED状态下的正常播放流程
                // V0.05B9n3e5g3n: 简化逻辑，统一走填充路径
                //
                // PAUSED + 不同节目：清空列表 + 填充siblings + 播放新节目
                // 未播放：空列表自动填充，有列表则复用
                //
                // 播放模式（R/C/Y）：
                //   - R (Repeat)：单曲循环，只播放当前曲目
                //   - C (Cycle)：从光标处顺序播放到末尾
                //   - Y (Random)：随机播放整个列表
                //
                // 光标同步：当前播放节目与L列表光标同步
                // ═══════════════════════════════════════════════════════════════════

                EVENT_LOG(fmt::format("Play: {}", node->title));
                record_play_history(node->url, node->title, node->duration);

                // 构建播放列表
                std::vector<std::string> playlist_urls;
                int current_idx = 0;
                bool has_video = false;

                // V0.05B9n3e5g3n: PAUSED状态 + 不同节目 = 强制填充路径
                if (saved_playlist.empty() || is_paused) {
                    // ═════════════════════════════════════════════════════════════
                    // 空列表 或 PAUSED状态：填充当前播客的全部节目到L列表
                    // 然后播放选中的节目
                    // ═════════════════════════════════════════════════════════════
                    playlist_urls = build_playlist_from_siblings_and_save(current_idx, has_video, node);
                    if (is_paused) {
                        EVENT_LOG(fmt::format("L-List Re-Fill (PAUSED): {} items, playing idx {}", playlist_urls.size(), current_idx));
                    } else {
                        EVENT_LOG(fmt::format("L-List Auto-Fill: {} items, playing idx {}", playlist_urls.size(), current_idx));
                    }
                } else {
                    // ═════════════════════════════════════════════════════════════
                    // 有内容：检查节目是否在列表中
                    // 若在列表：直接播放（光标跳转）
                    // 若不在列表：添加到末尾+播放
                    // ═════════════════════════════════════════════════════════════
                    int existing_idx = find_url_in_saved_playlist(node->url);
                    if (existing_idx >= 0) {
                        // 已在列表中，直接播放
                        current_idx = existing_idx;
                        EVENT_LOG(fmt::format("L-List: Found at idx {}, playing", current_idx));
                    } else {
                        // 不在列表中，添加到末尾
                        add_to_saved_playlist(node);
                        current_idx = static_cast<int>(saved_playlist.size()) - 1;
                        EVENT_LOG(fmt::format("L-List: Added to end, idx {}", current_idx));
                    }

                    // 构建播放URL列表
                    playlist_urls.clear();
                    current_playlist.clear();
                    for (size_t i = 0; i < saved_playlist.size(); ++i) {
                        const auto& item = saved_playlist[i];

                        // 检查本地缓存
                        std::string local = CacheManager::instance().get_local_file(item.url);
                        std::string play_url;
                        if (!local.empty() && fs::exists(local)) {
                            play_url = "file://" + local;
                        } else {
                            play_url = item.url;
                        }

                        playlist_urls.push_back(play_url);

                        PlaylistItem pl_item;
                        pl_item.title = item.title;
                        pl_item.url = play_url;
                        pl_item.duration = item.duration;
                        pl_item.is_video = item.is_video;
                        current_playlist.push_back(pl_item);

                        if (item.is_video) has_video = true;
                    }
                    current_playlist_index = current_idx;
                }

                // 同步L列表光标到当前播放项
                list_selected_idx = current_idx;

                // 应用播放模式处理
                auto processed = apply_play_mode(playlist_urls, current_idx, play_mode);

                // 执行播放
                execute_play(processed.urls, processed.start_idx, has_video, processed.loop_single);

                playback_node = node;
            }
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // 播放列表构建层
        // ═══════════════════════════════════════════════════════════════════════════

        // V0.05B9n3e5g3j: 查找URL在saved_playlist中的位置
        // 返回索引，如果未找到返回-1
        int find_url_in_saved_playlist(const std::string& url) {
            for (size_t i = 0; i < saved_playlist.size(); ++i) {
                if (saved_playlist[i].url == url) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        // V0.05B9n3e5g3j: 从兄弟节点构建播放列表并保存到L列表数据库
        // 这是空列表时自动填充的实现
        std::vector<std::string> build_playlist_from_siblings_and_save(int& current_idx, bool& has_video, TreeNodePtr current) {
            std::vector<std::string> urls;
            current_idx = 0;
            has_video = false;

            if (!current) return urls;

            // 获取父节点
            auto parent = current->parent.lock();

            // F模式特殊处理：收藏的单个节目
            if (mode == AppMode::FAVOURITE && parent && parent->title == "Root") {
                // 单个节目，只添加当前项
                std::string local = CacheManager::instance().get_local_file(current->url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : current->url;
                urls.push_back(play_url);

                // 保存到数据库
                DatabaseManager::instance().clear_playlist_table();
                SavedPlaylistItem item;
                item.title = current->title;
                item.url = current->url;
                item.duration = current->duration;
                URLType url_type = URLClassifier::classify(current->url);
                item.is_video = current->is_youtube || URLClassifier::is_video(url_type);
                item.sort_order = 0;
                DatabaseManager::instance().save_playlist_item(item);
                saved_playlist.clear();
                saved_playlist.push_back(item);

                // 构建current_playlist
                current_playlist.clear();
                PlaylistItem pl_item;
                pl_item.title = current->title;
                pl_item.url = play_url;
                pl_item.duration = current->duration;
                pl_item.is_video = item.is_video;
                current_playlist.push_back(pl_item);
                current_playlist_index = 0;
                has_video = item.is_video;

                EVENT_LOG(fmt::format("L-List: Single item saved"));
                return urls;
            }

            // 如果没有父节点或父节点无子节点，只添加当前项
            if (!parent || parent->children.empty()) {
                std::string local = CacheManager::instance().get_local_file(current->url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : current->url;
                urls.push_back(play_url);

                DatabaseManager::instance().clear_playlist_table();
                SavedPlaylistItem item;
                item.title = current->title;
                item.url = current->url;
                item.duration = current->duration;
                URLType url_type = URLClassifier::classify(current->url);
                item.is_video = current->is_youtube || URLClassifier::is_video(url_type);
                item.sort_order = 0;
                DatabaseManager::instance().save_playlist_item(item);
                saved_playlist.clear();
                saved_playlist.push_back(item);

                current_playlist.clear();
                PlaylistItem pl_item;
                pl_item.title = current->title;
                pl_item.url = play_url;
                pl_item.duration = current->duration;
                pl_item.is_video = item.is_video;
                current_playlist.push_back(pl_item);
                current_playlist_index = 0;
                has_video = item.is_video;

                return urls;
            }

            auto& siblings = parent->children;

            // 在兄弟节点中找到当前节点的索引
            size_t cursor_idx = 0;
            bool found = false;
            for (size_t i = 0; i < siblings.size(); ++i) {
                if (siblings[i].get() == current.get()) {
                    cursor_idx = i;
                    found = true;
                    break;
                }
            }

            if (!found) {
                // 未找到，只添加当前项
                std::string local = CacheManager::instance().get_local_file(current->url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : current->url;
                urls.push_back(play_url);

                DatabaseManager::instance().clear_playlist_table();
                SavedPlaylistItem item;
                item.title = current->title;
                item.url = current->url;
                item.duration = current->duration;
                URLType url_type = URLClassifier::classify(current->url);
                item.is_video = current->is_youtube || URLClassifier::is_video(url_type);
                item.sort_order = 0;
                DatabaseManager::instance().save_playlist_item(item);
                saved_playlist.clear();
                saved_playlist.push_back(item);

                current_playlist.clear();
                PlaylistItem pl_item;
                pl_item.title = current->title;
                pl_item.url = play_url;
                pl_item.duration = current->duration;
                pl_item.is_video = item.is_video;
                current_playlist.push_back(pl_item);
                current_playlist_index = 0;
                has_video = item.is_video;

                return urls;
            }

            // 清空数据库中的播放列表
            DatabaseManager::instance().clear_playlist_table();
            saved_playlist.clear();
            current_playlist.clear();

            // 收集所有兄弟节点并保存到数据库
            bool reversed = parent->sort_reversed;
            int save_order = 0;
            int actual_idx = 0;  // 当前播放项在saved_playlist中的实际索引

            if (!reversed) {
                // 正序
                for (size_t i = 0; i < siblings.size(); ++i) {
                    auto& sib = siblings[i];
                    if (!sib->url.empty()) {
                        std::string local = CacheManager::instance().get_local_file(sib->url);
                        std::string play_url = (!local.empty() && fs::exists(local)) ?
                            "file://" + local : sib->url;
                        urls.push_back(play_url);

                        // 保存到数据库
                        SavedPlaylistItem save_item;
                        save_item.title = sib->title;
                        save_item.url = sib->url;
                        save_item.duration = sib->duration;
                        URLType url_type = URLClassifier::classify(sib->url);
                        save_item.is_video = sib->is_youtube || URLClassifier::is_video(url_type);
                        save_item.sort_order = save_order;
                        DatabaseManager::instance().save_playlist_item(save_item);
                        saved_playlist.push_back(save_item);

                        // 构建current_playlist
                        PlaylistItem pl_item;
                        pl_item.title = sib->title;
                        pl_item.url = play_url;
                        pl_item.duration = sib->duration;
                        pl_item.is_video = save_item.is_video;
                        current_playlist.push_back(pl_item);

                        if (pl_item.is_video) has_video = true;

                        // 记录当前播放项的索引
                        if (i == cursor_idx) {
                            actual_idx = save_order;
                        }
                        save_order++;
                    }
                }
            } else {
                // 倒序：从末尾到开头
                for (int i = static_cast<int>(siblings.size()) - 1; i >= 0; --i) {
                    auto& sib = siblings[i];
                    if (!sib->url.empty()) {
                        std::string local = CacheManager::instance().get_local_file(sib->url);
                        std::string play_url = (!local.empty() && fs::exists(local)) ?
                            "file://" + local : sib->url;
                        urls.push_back(play_url);

                        // 保存到数据库
                        SavedPlaylistItem save_item;
                        save_item.title = sib->title;
                        save_item.url = sib->url;
                        save_item.duration = sib->duration;
                        URLType url_type = URLClassifier::classify(sib->url);
                        save_item.is_video = sib->is_youtube || URLClassifier::is_video(url_type);
                        save_item.sort_order = save_order;
                        DatabaseManager::instance().save_playlist_item(save_item);
                        saved_playlist.push_back(save_item);

                        // 构建current_playlist
                        PlaylistItem pl_item;
                        pl_item.title = sib->title;
                        pl_item.url = play_url;
                        pl_item.duration = sib->duration;
                        pl_item.is_video = save_item.is_video;
                        current_playlist.push_back(pl_item);

                        if (pl_item.is_video) has_video = true;

                        // 记录当前播放项的索引（倒序时计算）
                        if (static_cast<size_t>(i) == cursor_idx) {
                            actual_idx = save_order;
                        }
                        save_order++;
                    }
                }
            }

            current_idx = actual_idx;
            current_playlist_index = actual_idx;

            EVENT_LOG(fmt::format("L-List: Saved {} items, current at {}", save_order, actual_idx));
            return urls;
        }

        // 从saved_playlist构建完整播放列表（L模式）
        // 返回URL列表，current_idx为当前光标位置
        std::vector<std::string> build_playlist_from_saved(int& current_idx, bool& has_video, TreeNodePtr current_node) {
            std::vector<std::string> urls;
            current_idx = 0;
            has_video = false;
            
            // 定位当前URL在saved_playlist中的位置
            for (size_t i = 0; i < saved_playlist.size(); ++i) {
                if (saved_playlist[i].url == current_node->url || 
                    saved_playlist[i].url.find(current_node->url) != std::string::npos ||
                    current_node->url.find(saved_playlist[i].url) != std::string::npos) {
                    current_idx = static_cast<int>(i);
                    break;
                }
            }
            
            // 收集完整的URL列表（不截断）
            current_playlist.clear();
            for (size_t i = 0; i < saved_playlist.size(); ++i) {
                const auto& item = saved_playlist[i];
                urls.push_back(item.url);
                
                PlaylistItem pl_item;
                pl_item.title = item.title;
                pl_item.url = item.url;
                pl_item.duration = item.duration;
                pl_item.is_video = item.is_video;
                current_playlist.push_back(pl_item);
                
                if (item.is_video) has_video = true;
            }
            
            current_playlist_index = current_idx;
            return urls;
        }
        
        // 从兄弟节点构建完整播放列表（默认模式）
        // 收集当前光标所在节目列表的所有节目，current_idx为光标位置
        // V0.05B9n3e5g3d: F模式特殊处理
        //   - 收藏单个节目：parent->title == "Root"，返回单元素列表
        //   - 收藏播客节点后展开播放：parent是该播客节点，返回完整节目列表
        std::vector<std::string> build_playlist_from_siblings(int& current_idx, bool& has_video, TreeNodePtr current) {
            std::vector<std::string> urls;
            current_idx = 0;
            has_video = false;
            
            if (!current) return urls;
            
            // 获取父节点
            auto parent = current->parent.lock();
            
            // ═══════════════════════════════════════════════════════════════════════════
            // V0.05B9n3e5g3d: F模式特殊处理
            // 收藏的单个节目直接在fav_root下，parent->title == "Root"
            // 此时返回只包含该节目的单元素列表
            // ═══════════════════════════════════════════════════════════════════════════
            if (mode == AppMode::FAVOURITE && parent && parent->title == "Root") {
                LOG(fmt::format("[F-MODE] Single episode favourite: {}", current->title));
                
                std::string local = CacheManager::instance().get_local_file(current->url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : current->url;
                urls.push_back(play_url);
                
                current_playlist.clear();
                PlaylistItem pl_item;
                pl_item.title = current->title;
                pl_item.url = play_url;
                pl_item.duration = current->duration;
                URLType url_type = URLClassifier::classify(current->url);
                pl_item.is_video = current->is_youtube || URLClassifier::is_video(url_type);
                current_playlist.push_back(pl_item);
                current_playlist_index = 0;
                has_video = pl_item.is_video;
                
                EVENT_LOG(fmt::format("[F-MODE] Single item playlist: {}", current->title));
                return urls;
            }
            
            // 如果没有父节点，返回单元素列表
            if (!parent || parent->children.empty()) {
                std::string local = CacheManager::instance().get_local_file(current->url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : current->url;
                urls.push_back(play_url);
                
                current_playlist.clear();
                PlaylistItem pl_item;
                pl_item.title = current->title;
                pl_item.url = play_url;
                pl_item.duration = current->duration;
                URLType url_type = URLClassifier::classify(current->url);
                pl_item.is_video = current->is_youtube || URLClassifier::is_video(url_type);
                current_playlist.push_back(pl_item);
                current_playlist_index = 0;
                has_video = pl_item.is_video;
                
                return urls;
            }
            
            auto& siblings = parent->children;
            
            // 在兄弟节点中找到当前节点的索引
            size_t cursor_idx = 0;
            bool found = false;
            for (size_t i = 0; i < siblings.size(); ++i) {
                if (siblings[i].get() == current.get()) {
                    cursor_idx = i;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                // 未找到，返回单元素列表
                std::string local = CacheManager::instance().get_local_file(current->url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : current->url;
                urls.push_back(play_url);
                
                current_playlist.clear();
                PlaylistItem pl_item;
                pl_item.title = current->title;
                pl_item.url = play_url;
                pl_item.duration = current->duration;
                URLType url_type = URLClassifier::classify(current->url);
                pl_item.is_video = current->is_youtube || URLClassifier::is_video(url_type);
                current_playlist.push_back(pl_item);
                current_playlist_index = 0;
                has_video = pl_item.is_video;
                
                return urls;
            }
            
            // 清空当前播放列表，重建
            current_playlist.clear();
            
            // 收集所有兄弟节点（完整列表，不截断）
            // 根据父节点的sort_reversed决定遍历方向
            bool reversed = parent->sort_reversed;
            
            if (!reversed) {
                // 正序：从头到末尾
                for (size_t i = 0; i < siblings.size(); ++i) {
                    auto& sib = siblings[i];
                    if (!sib->url.empty()) {
                        std::string local = CacheManager::instance().get_local_file(sib->url);
                        std::string play_url = (!local.empty() && fs::exists(local)) ?
                            "file://" + local : sib->url;
                        urls.push_back(play_url);
                        
                        PlaylistItem pl_item;
                        pl_item.title = sib->title;
                        pl_item.url = play_url;
                        pl_item.duration = sib->duration;
                        URLType url_type = URLClassifier::classify(sib->url);
                        pl_item.is_video = sib->is_youtube || URLClassifier::is_video(url_type);
                        current_playlist.push_back(pl_item);
                        
                        if (pl_item.is_video) has_video = true;
                    }
                }
                // 正序时，current_idx为cursor_idx（但需要计算实际位置）
                current_idx = static_cast<int>(current_playlist.size()) > 0 ? 
                    static_cast<int>(cursor_idx) : 0;
            } else {
                // 倒序：从末尾到开头
                for (int i = static_cast<int>(siblings.size()) - 1; i >= 0; --i) {
                    auto& sib = siblings[i];
                    if (!sib->url.empty()) {
                        std::string local = CacheManager::instance().get_local_file(sib->url);
                        std::string play_url = (!local.empty() && fs::exists(local)) ?
                            "file://" + local : sib->url;
                        urls.push_back(play_url);
                        
                        PlaylistItem pl_item;
                        pl_item.title = sib->title;
                        pl_item.url = play_url;
                        pl_item.duration = sib->duration;
                        URLType url_type = URLClassifier::classify(sib->url);
                        pl_item.is_video = sib->is_youtube || URLClassifier::is_video(url_type);
                        current_playlist.push_back(pl_item);
                        
                        if (pl_item.is_video) has_video = true;
                    }
                }
                // 倒序时，current_idx为(总长度-1-cursor_idx)
                current_idx = static_cast<int>(current_playlist.size()) > 0 ?
                    static_cast<int>(siblings.size() - 1 - cursor_idx) : 0;
            }
            
            current_playlist_index = current_idx;
            return urls;
        }
        
        // ═══════════════════════════════════════════════════════════════════════════
        // 播放模式处理层
        // ═══════════════════════════════════════════════════════════════════════════
        
        // 根据播放模式处理播放列表
        // R (Repeat)：单曲循环，列表只包含当前项
        // C (Cycle)：顺序播放，从当前项开始
        // Y (Random)：随机播放，打乱整个列表
        ProcessedPlaylist apply_play_mode(const std::vector<std::string>& urls, int current_idx, PlayMode mode) {
            ProcessedPlaylist result;
            result.loop_single = false;
            
            if (urls.empty()) {
                result.start_idx = 0;
                return result;
            }
            
            // 确保current_idx在有效范围内
            if (current_idx < 0) current_idx = 0;
            if (current_idx >= static_cast<int>(urls.size())) current_idx = urls.size() - 1;
            
            switch (mode) {
                case PlayMode::SINGLE: {
                    // R (Repeat) 模式：单曲循环
                    // 列表只包含当前曲目，设置循环播放
                    result.urls.push_back(urls[current_idx]);
                    result.start_idx = 0;
                    result.loop_single = true;
                    EVENT_LOG(fmt::format("Play Mode R (Repeat): single track loop - '{}'", 
                        current_idx < static_cast<int>(current_playlist.size()) ? 
                        current_playlist[current_idx].title : "unknown"));
                    break;
                }
                
                case PlayMode::CYCLE: {
                    // C (Cycle) 模式：顺序播放
                    // 从当前项开始播放到列表末尾
                    for (size_t i = current_idx; i < urls.size(); ++i) {
                        result.urls.push_back(urls[i]);
                    }
                    result.start_idx = 0;
                    EVENT_LOG(fmt::format("Play Mode C (Cycle): {} items from idx {}", 
                        result.urls.size(), current_idx));
                    break;
                }
                
                case PlayMode::RANDOM: {
                    // Y (Random) 模式：随机播放
                    // 打乱整个列表，从第一首开始播放
                    std::vector<size_t> indices;
                    for (size_t i = 0; i < urls.size(); ++i) {
                        indices.push_back(i);
                    }
                    
                    // 随机打乱
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::shuffle(indices.begin(), indices.end(), gen);
                    
                    // 按打乱顺序构建播放列表
                    for (size_t i : indices) {
                        result.urls.push_back(urls[i]);
                    }
                    result.start_idx = 0;
                    EVENT_LOG(fmt::format("Play Mode Y (Random): {} items shuffled", result.urls.size()));
                    break;
                }
            }
            
            return result;
        }
        
        // ═══════════════════════════════════════════════════════════════════════════
        // 播放执行层
        // ═══════════════════════════════════════════════════════════════════════════
        
        // 执行播放
        // urls: 播放列表URL
        // start_idx: 起始播放位置
        // has_video: 是否包含视频
        // loop_single: 是否单曲循环（R模式）
        void execute_play(const std::vector<std::string>& urls, int start_idx, bool has_video, bool loop_single) {
            if (urls.empty()) return;
            
            if (urls.size() == 1) {
                // 单曲播放
                player.play(urls[0], has_video);
                if (loop_single) {
                    // R模式：设置单曲循环
                    player.set_loop_file(true);
                    EVENT_LOG("Execute: single track with loop");
                } else {
                    EVENT_LOG("Execute: single track");
                }
            } else {
                // 播放列表播放
                if (loop_single) {
                    // R模式但列表只有一个元素（已在上面处理）
                    player.play_list_from(urls, start_idx, has_video);
                } else {
                    // C/Y模式：播放列表
                    player.play_list_from(urls, start_idx, has_video);
                }
                EVENT_LOG(fmt::format("Execute: {} items, start from idx {}", urls.size(), start_idx));
            }
            
            LOG(fmt::format("[PLAY] Executed: {} items, start_idx={}, loop={}", 
                urls.size(), start_idx, loop_single));
        }
        
        void toggle_mark() {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            node->marked = !node->marked;
            EVENT_LOG(fmt::format("Mark: {}", node->marked ? "ON" : "OFF"));
            Persistence::save_cache(radio_root, podcast_root);
        }
        
        // V0.05B9n3e5g3n: 重写排序逻辑
        // 支持两种排序场景：
        // 1. L弹窗内：对saved_playlist排序
        // 2. 节点树：对同级节点（siblings）排序
        // 排序规则：第一次A→Z（数字在前），第二次Z→A（字母在前）
        // 数字开头始终排在前面（正序时在头部，反序时在尾部）
        
        // L列表排序状态（使用成员变量，非静态）
        bool saved_playlist_sort_reversed_ = false;
        
        // 排序比较函数：支持数字开头
        // 数字开头(0-9) < 字母开头(A-Z)
        static bool title_compare_asc(const std::string& a, const std::string& b) {
            bool a_starts_digit = !a.empty() && std::isdigit(static_cast<unsigned char>(a[0]));
            bool b_starts_digit = !b.empty() && std::isdigit(static_cast<unsigned char>(b[0]));
            
            // 数字开头的排在前面
            if (a_starts_digit && !b_starts_digit) return true;
            if (!a_starts_digit && b_starts_digit) return false;
            
            // 同类型：按字母顺序
            return a < b;
        }
        
        static bool title_compare_desc(const std::string& a, const std::string& b) {
            bool a_starts_digit = !a.empty() && std::isdigit(static_cast<unsigned char>(a[0]));
            bool b_starts_digit = !b.empty() && std::isdigit(static_cast<unsigned char>(b[0]));
            
            // 反序时：字母在前，数字在后
            if (a_starts_digit && !b_starts_digit) return false;
            if (!a_starts_digit && b_starts_digit) return true;
            
            // 同类型：按字母反序
            return a > b;
        }
        
        // L列表排序
        void toggle_saved_playlist_sort() {
            if (saved_playlist.empty()) {
                EVENT_LOG("Sort L-List: Empty list");
                return;
            }
            
            // 切换排序状态
            saved_playlist_sort_reversed_ = !saved_playlist_sort_reversed_;
            
            // 创建索引数组
            std::vector<size_t> indices(saved_playlist.size());
            for (size_t i = 0; i < indices.size(); ++i) {
                indices[i] = i;
            }
            
            // 排序索引
            if (saved_playlist_sort_reversed_) {
                // Z→A（字母在前，数字在后）
                std::sort(indices.begin(), indices.end(),
                    [this](size_t a, size_t b) {
                        return title_compare_desc(saved_playlist[a].title, saved_playlist[b].title);
                    });
            } else {
                // A→Z（数字在前，字母在后）
                std::sort(indices.begin(), indices.end(),
                    [this](size_t a, size_t b) {
                        return title_compare_asc(saved_playlist[a].title, saved_playlist[b].title);
                    });
            }
            
            // 重排saved_playlist
            std::vector<SavedPlaylistItem> new_playlist;
            new_playlist.reserve(saved_playlist.size());
            for (size_t idx : indices) {
                new_playlist.push_back(saved_playlist[idx]);
            }
            saved_playlist = std::move(new_playlist);
            
            // 更新数据库排序顺序
            DatabaseManager::instance().clear_playlist_table();
            for (size_t i = 0; i < saved_playlist.size(); ++i) {
                saved_playlist[i].sort_order = static_cast<int>(i);
                DatabaseManager::instance().save_playlist_item(saved_playlist[i]);
            }
            
            // 同步current_playlist
            current_playlist.clear();
            for (const auto& item : saved_playlist) {
                std::string local = CacheManager::instance().get_local_file(item.url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : item.url;
                
                PlaylistItem pl_item;
                pl_item.title = item.title;
                pl_item.url = play_url;
                pl_item.duration = item.duration;
                pl_item.is_video = item.is_video;
                current_playlist.push_back(pl_item);
            }
            
            // 重置光标到第一项
            list_selected_idx = 0;
            
            EVENT_LOG(fmt::format("Sort L-List: {} ({} items)", 
                saved_playlist_sort_reversed_ ? "Z→A" : "A→Z",
                saved_playlist.size()));
        }
        
        // 节点树排序
        void toggle_sort_order() {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            if (!node) return;
            
            // 找到父节点，对父节点的children排序
            auto parent = node->parent.lock();
            TreeNodePtr sort_target;
            
            if (parent) {
                sort_target = parent;
            } else {
                sort_target = current_root;
            }
            
            if (!sort_target || sort_target->children.empty()) {
                EVENT_LOG("Sort: No siblings to sort");
                return;
            }
            
            // 切换排序状态
            sort_target->sort_reversed = !sort_target->sort_reversed;
            
            // 执行排序
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                if (sort_target->sort_reversed) {
                    // Z→A（字母在前，数字在后）
                    std::sort(sort_target->children.begin(), sort_target->children.end(),
                        [](const TreeNodePtr& a, const TreeNodePtr& b) {
                            return title_compare_desc(a->title, b->title);
                        });
                } else {
                    // A→Z（数字在前，字母在后）
                    std::sort(sort_target->children.begin(), sort_target->children.end(),
                        [](const TreeNodePtr& a, const TreeNodePtr& b) {
                            return title_compare_asc(a->title, b->title);
                        });
                }
                
                // 重新构建display_list
                display_list.clear();
                flatten(current_root, 0, true, -1);
            }
            
            // 生成排序范围描述
            std::string scope_desc;
            if (sort_target == current_root) {
                scope_desc = mode == AppMode::RADIO ? "Radio List" : 
                             mode == AppMode::PODCAST ? "Podcast List" : "Current List";
            } else {
                scope_desc = sort_target->title.empty() ? "current list" : sort_target->title;
            }
            
            EVENT_LOG(fmt::format("Sort [{}]: {} ({} items)", 
                scope_desc, 
                sort_target->sort_reversed ? "Z→A" : "A→Z",
                sort_target->children.size()));
            
            Persistence::save_cache(radio_root, podcast_root);
        }
        
        int count_marked_safe(const TreeNodePtr& node) {
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            return count_marked(node);
        }
        
        int count_marked(const TreeNodePtr& node) {
            int count = node->marked ? 1 : 0;
            for (auto& child : node->children) count += count_marked(child);
            return count;
        }
        
        //修复多选标记 - 支持所有节点类型
        void collect_playable_marked(const TreeNodePtr& node, std::vector<TreeNodePtr>& list) {
            if (node->marked) {
                // 直接可播放类型
                if (node->type == NodeType::RADIO_STREAM || node->type == NodeType::PODCAST_EPISODE) {
                    list.push_back(node);
                }
                //文件夹/Feed类型 - 递归收集子节点中可播放项
                else if (node->type == NodeType::FOLDER || node->type == NodeType::PODCAST_FEED) {
                    for (auto& child : node->children) {
                        collect_playable_items(child, list);
                    }
                }
            }
            // 继续检查子节点
            for (auto& child : node->children) {
                collect_playable_marked(child, list);
            }
        }
        
        //辅助函数 - 收集节点中所有可播放项（不考虑标记状态）
        void collect_playable_items(const TreeNodePtr& node, std::vector<TreeNodePtr>& list) {
            if (node->type == NodeType::RADIO_STREAM || node->type == NodeType::PODCAST_EPISODE) {
                list.push_back(node);
            }
            for (auto& child : node->children) {
                collect_playable_items(child, list);
            }
        }
        
        void clear_marks(const TreeNodePtr& node) {
            node->marked = false;
            for (auto& child : node->children) clear_marks(child);
        }
        
        void reset_search() {
            search_query.clear();
            search_matches.clear();
            current_match_idx = -1;
            total_matches = 0;
        }
        
        //支持ESC取消搜索
        void perform_search() {
            std::string q = ui.input_box("Search:");
            //检查用户是否取消
            if (UI::is_input_cancelled(q)) {
                EVENT_LOG("Search cancelled");
                return;
            }
            if (q.empty()) return;
            reset_search();
            search_query = q;
            std::string ql = Utils::to_lower(q);
            search_recursive(current_root, ql, search_matches);
            total_matches = search_matches.size();
            if (total_matches > 0) {
                current_match_idx = 0;
                jump_to_match(0);
                EVENT_LOG(fmt::format("Found {} matches", total_matches));
            } else {
                EVENT_LOG("No matches found");
            }
        }
        
        void search_recursive(const TreeNodePtr& node, const std::string& query, std::vector<TreeNodePtr>& results) {
            if (Utils::to_lower(node->title).find(query) != std::string::npos)
                results.push_back(node);
            for (auto& child : node->children) search_recursive(child, query, results);
        }
        
        void jump_search(int dir) {
            if (total_matches == 0) return;
            current_match_idx = (current_match_idx + dir + total_matches) % total_matches;
            jump_to_match(current_match_idx);
        }
        
        void jump_to_match(int idx) {
            if (idx < 0 || idx >= total_matches) return;
            auto node = search_matches[idx];
            reveal_node(node);
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                display_list.clear();
                flatten(current_root, 0, true, -1);
            }
            for (int i = 0; i < (int)display_list.size(); ++i) {
                if (display_list[i].node == node) {
                    selected_idx = i;
                    view_start = std::max(0, selected_idx - (LINES - 5) / 2);
                    return;
                }
            }
        }
        
        void reveal_node(TreeNodePtr node) {
            std::function<bool(TreeNodePtr)> reveal = [&](TreeNodePtr curr) -> bool {
                if (curr == node) return true;
                for (auto& child : curr->children) {
                    if (reveal(child)) {
                        curr->expanded = true;
                        return true;
                    }
                }
                return false;
            };
            reveal(current_root);
        }
        
        //完善delete_node支持所有模式
        //ONLINE模式删除增强，彻底清理数据库记录
        //ONLINE/HISTORY模式支持多选删除
        void delete_node(int marked_count) {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            
            //ONLINE模式 - 支持多选删除
            if (mode == AppMode::ONLINE) {
                // 多选删除
                if (marked_count > 0) {
                    std::string response = ui.dialog(fmt::format("Delete {} marked items? (Y/N)", marked_count));
                    if (response != "Y" && response != "y") return;
                    
                    std::vector<TreeNodePtr> to_delete;
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        collect_marked(OnlineState::instance().online_root, to_delete);
                    }
                    
                    int deleted_count = 0;
                    for (auto& n : to_delete) {
                        // 根据节点类型执行不同的删除逻辑
                        if (n->url.find("search:") == 0) {
                            // 搜索记录节点
                            std::string search_id = n->url.substr(7);
                            size_t pos = search_id.find(':');
                            if (pos != std::string::npos) {
                                //修复URL格式解析 - search:region:query
                                std::string region = search_id.substr(0, pos);
                                std::string query = search_id.substr(pos + 1);
                                DatabaseManager::instance().delete_search_history(query, region);
                            }
                            // 删除子节点的缓存
                            for (auto& child : n->children) {
                                if (child->type == NodeType::PODCAST_FEED && !child->url.empty()) {
                                    DatabaseManager::instance().delete_podcast_cache(child->url);
                                    DatabaseManager::instance().delete_episode_cache_by_feed(child->url);
                                }
                            }
                        } else if (n->type == NodeType::PODCAST_FEED && !n->url.empty()) {
                            // 播客Feed节点
                            DatabaseManager::instance().delete_podcast_cache(n->url);
                            DatabaseManager::instance().delete_episode_cache_by_feed(n->url);
                        }
                        
                        // 从树中删除
                        {
                            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                            remove_node(OnlineState::instance().online_root, n);
                        }
                        deleted_count++;
                    }
                    
                    // 清除所有标记
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        clear_marks(OnlineState::instance().online_root);
                    }
                    
                    EVENT_LOG(fmt::format("Deleted {} items from ONLINE", deleted_count));
                    if (selected_idx > 0) selected_idx--;
                    return;
                }
                
                // 单选删除 - 判断节点类型
                if (node->url.find("search:") == 0) {
                    // 搜索记录节点 - 删除整个搜索记录及其子节点的缓存
                    std::string response = ui.dialog("Delete this search record and all caches? (Y/N)");
                    if (response == "Y" || response == "y") {
                        //修复URL格式解析 - search:region:query
                        std::string search_id = node->url.substr(7);  // 去掉"search:"
                        size_t pos = search_id.find(':');
                        if (pos != std::string::npos) {
                            std::string region = search_id.substr(0, pos);
                            std::string query = search_id.substr(pos + 1);
                            DatabaseManager::instance().delete_search_history(query, region);
                        }
                        
                        //删除所有子节点的播客缓存和节目缓存
                        for (auto& child : node->children) {
                            if (child->type == NodeType::PODCAST_FEED && !child->url.empty()) {
                                DatabaseManager::instance().delete_podcast_cache(child->url);
                                DatabaseManager::instance().delete_episode_cache_by_feed(child->url);
                            }
                        }
                        
                        // 从树中删除节点
                        {
                            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                            remove_node(OnlineState::instance().online_root, node);
                        }
                        EVENT_LOG(fmt::format("Deleted search record and caches: {}", node->title));
                        if (selected_idx > 0) selected_idx--;
                    }
                } else if (node->type == NodeType::PODCAST_FEED) {
                    // 播客Feed节点 - 删除播客缓存和节目缓存
                    std::string response = ui.dialog("Delete this podcast cache? (Y/N)");
                    if (response == "Y" || response == "y") {
                        // 删除数据库记录
                        if (!node->url.empty()) {
                            DatabaseManager::instance().delete_podcast_cache(node->url);
                            DatabaseManager::instance().delete_episode_cache_by_feed(node->url);
                        }
                        // 从树中删除节点
                        {
                            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                            remove_node(OnlineState::instance().online_root, node);
                        }
                        EVENT_LOG(fmt::format("Deleted podcast cache: {}", node->title));
                        if (selected_idx > 0) selected_idx--;
                    }
                } else {
                    // 其他节点 - 仅从树中删除
                    std::string response = ui.dialog("Delete this item? (Y/N)");
                    if (response == "Y" || response == "y") {
                        {
                            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                            remove_node(OnlineState::instance().online_root, node);
                        }
                        EVENT_LOG(fmt::format("Deleted: {}", node->title));
                        if (selected_idx > 0) selected_idx--;
                    }
                }
                return;
            }
            
            //HISTORY模式 - 支持多选删除
            if (mode == AppMode::HISTORY) {
                // 多选删除
                if (marked_count > 0) {
                    std::string response = ui.dialog(fmt::format("Delete {} marked history records? (Y/N)", marked_count));
                    if (response != "Y" && response != "y") return;
                    
                    std::vector<TreeNodePtr> to_delete;
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        collect_marked(history_root, to_delete);
                    }
                    
                    int deleted_count = 0;
                    for (auto& n : to_delete) {
                        // 从数据库删除
                        if (!n->url.empty()) {
                            DatabaseManager::instance().delete_history(n->url);
                        }
                        // 从树中删除
                        {
                            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                            remove_node(history_root, n);
                        }
                        deleted_count++;
                    }
                    
                    // 清除所有标记
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        clear_marks(history_root);
                    }
                    
                    EVENT_LOG(fmt::format("Deleted {} history records", deleted_count));
                    if (selected_idx > 0) selected_idx--;
                    return;
                }
                
                // 单选删除
                std::string response = ui.dialog("Delete this history record? (Y/N)");
                if (response == "Y" || response == "y") {
                    // 从数据库删除
                    DatabaseManager::instance().delete_history(node->url);
                    // 从树中删除节点
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        remove_node(history_root, node);
                    }
                    EVENT_LOG(fmt::format("Deleted history: {}", node->title));
                    if (selected_idx > 0) selected_idx--;
                }
                return;
            }
            
            // PODCAST/FAVOURITE模式
            if (mode != AppMode::PODCAST && mode != AppMode::FAVOURITE) return;
            
            //FAVOURITE模式 - 处理LINK节点的双向同步删除
            if (mode == AppMode::FAVOURITE) {
                // 检查是否在LINK节点下操作
                TreeNodePtr parent_link = nullptr;
                for (auto& f : fav_root->children) {
                    if (f->is_link) {
                        // 检查node是否是f的子节点或f本身
                        if (f.get() == node.get()) {
                            // 删除的是LINK节点本身 - 直接删除收藏
                            std::string response = ui.dialog("Remove this LINK from favourites? (Y/N)");
                            if (response == "Y" || response == "y") {
                                {
                                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                                    auto it = std::remove_if(fav_root->children.begin(), fav_root->children.end(), 
                                        [&](auto& n) { return n == node; });
                                    fav_root->children.erase(it, fav_root->children.end());
                                }
                                DatabaseManager::instance().delete_favourite(node->url);
                                EVENT_LOG(fmt::format("Removed LINK: {}", node->title));
                                if (selected_idx > 0) selected_idx--;
                            }
                            return;
                        }
                        // 检查node是否是f的子节点
                        for (auto& child : f->children) {
                            if (child.get() == node.get()) {
                                parent_link = f;
                                break;
                            }
                        }
                        if (parent_link) break;
                    }
                }
                
                // 如果在LINK节点下删除子节点，需要同步到ONLINE
                if (parent_link && parent_link->url == "online_root") {
                    // 这是online_root的LINK，删除需要同步
                    std::string response = ui.dialog("Delete from both FAVOURITE and ONLINE? (Y/N)");
                    if (response != "Y" && response != "y") return;
                    
                    // 从ONLINE的online_root删除
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        remove_node(OnlineState::instance().online_root, node);
                    }
                    
                    // 如果是搜索记录，删除数据库记录
                    if (node->url.find("search:") == 0) {
                        std::string search_id = node->url.substr(7);
                        size_t pos = search_id.find(':');
                        if (pos != std::string::npos) {
                            std::string region = search_id.substr(0, pos);
                            std::string query = search_id.substr(pos + 1);
                            DatabaseManager::instance().delete_search_history(query, region);
                        }
                    }
                    
                    // 清空LINK节点的children，下次展开重新同步
                    parent_link->children_loaded = false;
                    parent_link->children.clear();
                    
                    EVENT_LOG(fmt::format("Deleted from both: {}", node->title));
                    if (selected_idx > 0) selected_idx--;
                    return;
                }
            }
            
            //支持多选批量删除订阅
            if (marked_count > 0) {
                std::vector<TreeNodePtr> to_delete;
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    collect_marked(current_root, to_delete);
                }
                
                if (to_delete.empty()) return;
                
                std::string response = ui.dialog(fmt::format("Delete {} subscriptions? (Y/N)", to_delete.size()));
                if (response != "Y" && response != "y") return;
                
                int deleted_count = 0;
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    for (auto& n : to_delete) {
                        // 如果是PODCAST_FEED，删除订阅和缓存
                        if (n->type == NodeType::PODCAST_FEED && !n->url.empty()) {
                            DatabaseManager::instance().delete_podcast_cache(n->url);
                            DatabaseManager::instance().delete_episode_cache_by_feed(n->url);
                        }
                        remove_node(current_root, n);
                        deleted_count++;
                    }
                    clear_marks(current_root);
                }
                
                EVENT_LOG(fmt::format("Deleted {} subscriptions", deleted_count));
                save_persistent_data();
                Persistence::save_cache(radio_root, podcast_root);
                if (selected_idx > 0) selected_idx--;
                return;
            }
            
            if (node->type == NodeType::PODCAST_FEED) {
                std::string response = ui.dialog("Delete: (S)ubscription / (C)ache / (N)o?");
                if (response == "S" || response == "s") {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    remove_node(current_root, node);
                    EVENT_LOG(fmt::format("Deleted subscription: {}", node->title));
                } else if (response == "C" || response == "c") {
                    clear_feed_cache(node);
                    EVENT_LOG(fmt::format("Cleared cache for: {}", node->title));
                }
            } else if (node->type == NodeType::PODCAST_EPISODE) {
                std::string response = ui.dialog("Clear cache? (Y/N)");
                if (response == "Y" || response == "y") {
                    CacheManager::instance().clear_download(node->url);
                    node->is_downloaded = false;
                    node->local_file.clear();
                    EVENT_LOG(fmt::format("Cleared cache: {}", node->title));
                }
            } else {
                std::vector<TreeNodePtr> to_delete;
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    if (marked_count > 0) collect_marked(current_root, to_delete);
                    else to_delete.push_back(node);
                }
                if (to_delete.empty() || ui.dialog(fmt::format("Delete {} items? (Y/N)", to_delete.size())) != "Y") return;
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    for (auto& n : to_delete) remove_node(current_root, n);
                    clear_marks(current_root);
                }
                EVENT_LOG(fmt::format("Deleted {} items", to_delete.size()));
            }
            
            save_persistent_data();
            Persistence::save_cache(radio_root, podcast_root);
            if (selected_idx > 0) selected_idx--;
        }
        
        void clear_feed_cache(TreeNodePtr feed) {
            for (auto& child : feed->children) {
                CacheManager::instance().clear_cache(child->url);
                CacheManager::instance().clear_download(child->url);
                child->is_cached = false;
                child->is_downloaded = false;
                child->local_file.clear();
            }
            feed->is_cached = false;
            CacheManager::instance().clear_cache(feed->url);
            CacheManager::instance().save();
        }
        
        void collect_marked(const TreeNodePtr& node, std::vector<TreeNodePtr>& list) {
            if (node->marked) list.push_back(node);
            for (auto& child : node->children) collect_marked(child, list);
        }
        
        bool remove_node(TreeNodePtr parent, TreeNodePtr target) {
            auto& children = parent->children;
            auto it = std::remove_if(children.begin(), children.end(), [&](auto& n) { return n == target; });
            if (it != children.end()) { children.erase(it, children.end()); return true; }
            for (auto& child : parent->children) if (remove_node(child, target)) return true;
            return false;
        }
        
        void download_node(int marked_count) {
            std::string dir = Utils::get_download_dir();
            fs::create_directories(dir);
            
            std::vector<TreeNodePtr> items;
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                if (marked_count > 0) collect_playable_marked(current_root, items);
                else if (selected_idx < (int)display_list.size()) {
                    auto n = display_list[selected_idx].node;
                    if (n->type == NodeType::RADIO_STREAM || n->type == NodeType::PODCAST_EPISODE)
                        items.push_back(n);
                }
            }
            
            if (items.empty()) {
                EVENT_LOG("No downloadable items");
                return;
            }
            
            EVENT_LOG(fmt::format("Downloading {} items...", items.size()));
            
            for (auto& n : items) {
                std::string base_name = Utils::sanitize_filename(n->title);
                URLType url_type = URLClassifier::classify(n->url);
                bool is_youtube = URLClassifier::is_youtube(url_type);
                bool is_video_file = (url_type == URLType::VIDEO_FILE);
                
                std::string dl_id = ProgressManager::instance().start_download(n->title, n->url, is_youtube);
                
                if (is_youtube) {
                    //YouTube下载（带进度解析）
                    std::thread t([this, url = n->url, base_name, dir, title = n->title, dl_id, n]() {
                        EVENT_LOG(fmt::format("YouTube DL: {}", title));
                        ProgressManager::instance().update(dl_id, 5, "Fetching info...", "...", 0, 0, 0);
                        
                        //使用popen读取yt-dlp进度输出
                        std::string cmd = fmt::format(
                            "yt-dlp -f \"bestvideo+bestaudio/best\" --merge-output-format mp4 "
                            "-o \"{}/{}.mp4\" --no-warnings --newline --progress --no-playlist \"{}\" 2>&1",
                            dir, base_name, url);
                        
                        LOG(fmt::format("Executing: {}", cmd));
                        
                        FILE* pipe = popen(cmd.c_str(), "r");
                        bool success = false;
                        
                        if (pipe) {
                            char buffer[512];
                            std::string last_progress;
                            
                            while (fgets(buffer, sizeof(buffer), pipe)) {
                                std::string line = buffer;
                                
                                //解析yt-dlp进度输出
                                // 格式示例: [download]  45.2% of 123.45MiB at 1.23MiB/s ETA 00:45
                                if (line.find("[download]") != std::string::npos) {
                                    // 解析百分比
                                    size_t pct_pos = line.find('%');
                                    if (pct_pos != std::string::npos) {
                                        // 向前找数字
                                        size_t num_start = pct_pos;
                                        while (num_start > 0 && (isdigit(line[num_start-1]) || line[num_start-1] == '.')) {
                                            num_start--;
                                        }
                                        std::string pct_str = line.substr(num_start, pct_pos - num_start);
                                        try {
                                            int percent = static_cast<int>(std::stod(pct_str));
                                            if (percent > 100) percent = 100;
                                            
                                            // 解析速度
                                            std::string speed = "...";
                                            size_t speed_pos = line.find(" at ");
                                            if (speed_pos != std::string::npos) {
                                                size_t speed_end = line.find(" ETA", speed_pos);
                                                if (speed_end != std::string::npos) {
                                                    speed = line.substr(speed_pos + 4, speed_end - speed_pos - 4);
                                                    // 清理空白
                                                    while (!speed.empty() && isspace(speed.front())) speed.erase(0, 1);
                                                    while (!speed.empty() && isspace(speed.back())) speed.pop_back();
                                                }
                                            }
                                            
                                            // 解析ETA
                                            int eta_seconds = 0;
                                            size_t eta_pos = line.find("ETA");
                                            if (eta_pos != std::string::npos) {
                                                std::string eta_str = line.substr(eta_pos + 4);
                                                // 清理空白
                                                while (!eta_str.empty() && isspace(eta_str.front())) eta_str.erase(0, 1);
                                                while (!eta_str.empty() && (isspace(eta_str.back()) || eta_str.back() == '\n' || eta_str.back() == '\r')) {
                                                    eta_str.pop_back();
                                                }
                                                // 解析ETA格式 (MM:SS 或 HH:MM:SS)
                                                int h = 0, m = 0, s = 0;
                                                if (sscanf(eta_str.c_str(), "%d:%d:%d", &h, &m, &s) == 3) {
                                                    eta_seconds = h * 3600 + m * 60 + s;
                                                } else if (sscanf(eta_str.c_str(), "%d:%d", &m, &s) == 2) {
                                                    eta_seconds = m * 60 + s;
                                                }
                                            }
                                            
                                            ProgressManager::instance().update(
                                                dl_id, percent, "Downloading", speed, eta_seconds, 0, 0);
                                        } catch (...) {}
                                    }
                                } else if (line.find("[Merger]") != std::string::npos) {
                                    ProgressManager::instance().update(dl_id, 95, "Merging...", "...", 0, 0, 0);
                                } else if (line.find("has already") != std::string::npos) {
                                    ProgressManager::instance().update(dl_id, 100, "Already exists", "", 0, 0, 0);
                                    success = true;
                                }
                            }
                            
                            int exit_status = pclose(pipe);
                            success = (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0);
                        } else {
                            // 如果popen失败，回退到简单模式
                            std::string fallback_cmd = fmt::format(
                                "yt-dlp -f \"bestvideo+bestaudio/best\" --merge-output-format mp4 "
                                "-o \"{}/{}.mp4\" --no-warnings --no-playlist \"{}\" >/dev/null 2>&1",
                                dir, base_name, url);
                            int exit_code = system(fallback_cmd.c_str());
                            success = (exit_code == 0);
                        }
                        
                        ProgressManager::instance().complete(dl_id, success);
                        if (success) {
                            std::string local_file = dir + "/" + base_name + ".mp4";
                            CacheManager::instance().mark_downloaded(url, local_file);
                            n->is_downloaded = true;
                            n->local_file = local_file;
                            EVENT_LOG(fmt::format("Saved: {}.mp4", base_name));
                        } else {
                            EVENT_LOG(fmt::format("Failed: {}", title));
                        }
                        Persistence::save_cache(radio_root, podcast_root);
                    });
                    t.detach();
                } else {
                    //普通下载（带进度回调）
                    std::thread t([this, url = n->url, dir, base_name, title = n->title, dl_id, is_video_file, n]() {
                        std::string ext = is_video_file ? ".mp4" : ".mp3";
                        size_t p = url.find_last_of('.');
                        if (p != std::string::npos && p > url.find_last_of('/')) {
                            std::string url_ext = url.substr(p);
                            if (url_ext.length() <= 5 && url_ext.find("?") == std::string::npos) ext = url_ext;
                        }
                        
                        std::string filepath = dir + "/" + base_name + ext;
                        EVENT_LOG(fmt::format("Downloading: {}", title));
                        
                        CURL* curl = curl_easy_init();
                        FILE* f = fopen(filepath.c_str(), "wb");
                        bool success = false;
                        
                        if (curl && f) {
                            //设置进度回调数据
                            CurlProgressData progress_data;
                            progress_data.dl_id = dl_id;
                            progress_data.title = title;
                            progress_data.last_bytes = 0;
                            progress_data.last_time = std::chrono::steady_clock::now();
                            
                            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                            curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
                            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                            curl_easy_setopt(curl, CURLOPT_USERAGENT, "PODRADIO");
                            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
                            //下载也需要完整的SSL选项
                            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
                            curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
                            
                            //启用进度回调
                            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress_callback);
                            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
                            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                            
                            // 初始状态
                            ProgressManager::instance().update(dl_id, 0, "Connecting...", "...", 0, 0, 0);
                            
                            success = (curl_easy_perform(curl) == CURLE_OK);
                            curl_easy_cleanup(curl);
                            fclose(f);
                        } else {
                            if (f) fclose(f);
                            if (curl) curl_easy_cleanup(curl);
                        }
                        
                        ProgressManager::instance().complete(dl_id, success);
                        if (success) {
                            CacheManager::instance().mark_downloaded(url, filepath);
                            n->is_downloaded = true;
                            n->local_file = filepath;
                            EVENT_LOG(fmt::format("Saved: {}{}", base_name, ext));
                        } else {
                            EVENT_LOG(fmt::format("Failed: {}", title));
                        }
                        Persistence::save_cache(radio_root, podcast_root);
                    });
                    t.detach();
                }
            }
            
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                clear_marks(current_root);
            }
            Persistence::save_cache(radio_root, podcast_root);
        }
        
        //扩展收藏功能，支持所有节点类型（包括文件夹）
        //ONLINE模式收藏区分两种类型，双向同步
        void add_favourite() {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;

            // ═══════════════════════════════════════════════════════════════════
            //统一LINK机制 - 所有收藏都是LINK
            // 收藏节点存储：目标URL + 目标类型 + 来源模式 + 运行时引用
            // 展开时：运行时引用 → 内存查找 → 数据库缓存 → 网络解析
            // ═══════════════════════════════════════════════════════════════════

            // 检查是否已存在相同URL的收藏
            for (auto& f : fav_root->children) {
                if (!node->url.empty() && f->url == node->url) {
                    EVENT_LOG(fmt::format("★ Already in favourites: {}", node->title));
                    return;
                }
                if (node->url.empty() && f->title == node->title && f->type == node->type) {
                    EVENT_LOG(fmt::format("★ Already in favourites: {}", node->title));
                    return;
                }
            }

            // 获取来源模式名称
            std::string source_mode_name;
            switch (mode) {
                case AppMode::RADIO: source_mode_name = "RADIO"; break;
                case AppMode::PODCAST: source_mode_name = "PODCAST"; break;
                case AppMode::ONLINE: source_mode_name = "ONLINE"; break;
                case AppMode::FAVOURITE: source_mode_name = "FAVOURITE"; break;
                case AppMode::HISTORY: source_mode_name = "HISTORY"; break;
            }

            // 创建LINK节点
            auto fn = std::make_shared<TreeNode>();
            fn->title = node->title;
            fn->url = node->url;                    // 目标URL（用于定位和重建）
            fn->type = node->type;                  // 目标类型
            fn->is_link = true;                     // 标记为LINK
            fn->link_target_url = node->url;        // 持久化目标URL
            fn->source_mode = source_mode_name;     // 来源模式
            fn->linked_node = node;                 // 运行时引用（可能失效）
            fn->is_youtube = node->is_youtube;
            fn->channel_name = node->channel_name;
            fn->children_loaded = false;            // LINK节点初始不加载

            // 特殊处理：Online Search根节点
            if (node->url == "online_root" ||
                node.get() == OnlineState::instance().online_root.get()) {
                fn->url = "online_root";
                fn->link_target_url = "online_root";
                fn->linked_node = OnlineState::instance().online_root;
            }

            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                fn->parent = fav_root;
                fav_root->children.insert(fav_root->children.begin(), fn);
            }

            // 保存到数据库（包含LINK信息）
            json link_info;
            link_info["is_link"] = true;
            link_info["source_mode"] = source_mode_name;
            link_info["link_target_url"] = fn->link_target_url;
            std::string data_json = link_info.dump();

            DatabaseManager::instance().save_favourite(
                fn->title, fn->url, (int)fn->type,
                fn->is_youtube, fn->channel_name, source_mode_name, data_json
            );

            EVENT_LOG(fmt::format("★ Favourite LINK: {} [{}]", node->title, source_mode_name));
        }
        
        //从SQLite加载历史记录到history_root
        void load_history_to_root() {
            auto history = DatabaseManager::instance().get_history(100);  // 获取最近100条
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            history_root->children.clear();
            
            for (const auto& [url, title, timestamp] : history) {
                auto node = std::make_shared<TreeNode>();
                node->title = title.empty() ? "Unknown" : title;
                node->url = url;
                // 根据URL判断类型
                URLType url_type = URLClassifier::classify(url);
                if (URLClassifier::is_video(url_type)) {
                    node->type = NodeType::PODCAST_EPISODE;
                    node->is_youtube = true;
                } else if (url.find(".mp3") != std::string::npos || 
                           url.find(".aac") != std::string::npos ||
                           url.find(".m3u8") != std::string::npos) {
                    node->type = NodeType::RADIO_STREAM;
                } else {
                    node->type = NodeType::PODCAST_EPISODE;
                }
                node->children_loaded = true;
                node->subtext = timestamp;  //用subtext存储时间戳
                history_root->children.push_back(node);
            }
        }
        
        //记录播放历史
        void record_play_history(const std::string& url, const std::string& title, int duration = 0) {
            if (url.empty()) return;
            DatabaseManager::instance().add_history(url, title, duration);
            // 重新加载历史记录
            load_history_to_root();
        }
        
        // V2.39-FF: 编辑节点标题和URL
        void edit_node() {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            
            // 只允许编辑播客订阅源和自定义节点
            if (node->type != NodeType::PODCAST_FEED && node->type != NodeType::RADIO_STREAM) {
                EVENT_LOG("Can only edit feed/stream nodes");
                return;
            }
            
            // 获取新标题（显示当前值作为默认值）
            std::string new_title = ui.input_box("Title:", node->title);
            if (new_title.empty()) new_title = node->title;  // 保持原值
            
            // 获取新URL（显示当前值作为默认值）
            std::string new_url = ui.input_box("URL:", node->url);
            if (new_url.empty()) new_url = node->url;  // 保持原值
            
            // 更新节点
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                node->title = new_title;
                node->url = new_url;
            }
            
            EVENT_LOG(fmt::format("Updated: {} -> {}", new_title, new_url.substr(0, 50)));
            Persistence::save_cache(radio_root, podcast_root);
        }
        
        void refresh_node() {
            if (selected_idx >= (int)display_list.size()) return;
            auto node = display_list[selected_idx].node;
            
            if (node->type == NodeType::FOLDER && mode == AppMode::RADIO) {
                node->children.clear();
                node->children_loaded = false;
                node->loading = true;
                node->parse_failed = false;
                spawn_load_radio(node, true);
            } else if (node->type == NodeType::PODCAST_FEED) {
                node->children.clear();
                node->children_loaded = false;
                node->loading = true;
                node->parse_failed = false;
                spawn_load_feed(node);
                //打印完整URL
                EVENT_LOG(fmt::format("Refreshing: {}", node->url));
            } else if (node->type == NodeType::PODCAST_EPISODE) {
                CacheManager::instance().clear_cache(node->url);
                CacheManager::instance().clear_download(node->url);
                node->is_cached = false;
                node->is_downloaded = false;
                node->local_file.clear();
                EVENT_LOG(fmt::format("Cleared cache: {}", node->title));
            }
        }
        
        void add_feed() {
            std::string url = ui.input_box("Enter URL (RSS/YouTube @Channel):");
            if (url.empty()) return;
            
            URLType url_type = URLClassifier::classify(url);
            EVENT_LOG(fmt::format("Adding: {} [{}]", url, URLClassifier::type_name(url_type)));
            
            auto node = std::make_shared<TreeNode>();
            if (url_type == URLType::YOUTUBE_CHANNEL) {
                node->title = URLClassifier::extract_channel_name(url);
            } else {
                node->title = "Loading...";
            }
            node->url = url;
            node->type = NodeType::PODCAST_FEED;
            // 不设置loading=true，让节点显示正常的类型图标
            // spawn_load_feed会在解析开始时设置loading状态
            node->parse_failed = false;
            
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                node->parent = podcast_root;  //设置父节点指针
                podcast_root->children.insert(podcast_root->children.begin(), node);
            }
            
            //立即保存订阅到数据库（在解析之前就保存，确保重启后能加载）
            Persistence::save_data(podcast_root->children, fav_root->children);
            EVENT_LOG(fmt::format("Subscription saved: {}", url));
            
            spawn_load_feed(node);
        }
        
        void load_persistent_data() {
            std::vector<TreeNodePtr> podcasts, favs;
            Persistence::load_data(podcasts, favs);
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            if (!podcast_root->children_loaded) {
                for (auto& n : podcasts) {
                    n->parent = podcast_root;  //设置父节点指针
                    podcast_root->children.push_back(n);
                }
                podcast_root->children_loaded = true;
            }
            for (auto& n : favs) {
                n->parent = fav_root;  //设置父节点指针
                fav_root->children.push_back(n);
            }
        }
        
        void save_persistent_data() {
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            Persistence::save_data(podcast_root->children, fav_root->children);
        }
        
        //恢复上次播放状态
        //扩展恢复UI状态
        void restore_player_state() {
            auto saved_state = DatabaseManager::instance().load_player_state();
            
            // 恢复音量
            if (saved_state.volume >= 0 && saved_state.volume <= 100) {
                player.set_volume(saved_state.volume);
                EVENT_LOG(fmt::format("Restored volume: {}%", saved_state.volume));
            }
            
            // 恢复播放速度
            if (saved_state.speed >= 0.5 && saved_state.speed <= 2.0) {
                player.set_speed(saved_state.speed);
                EVENT_LOG(fmt::format("Restored speed: {:.2f}x", saved_state.speed));
            }
            
            // 恢复播放位置（如果有正在播放的媒体)
            if (!saved_state.current_url.empty() && saved_state.position > 0) {
                // 尝试恢复播放
                // 注意：不自动开始播放，只恢复位置信息
                EVENT_LOG(fmt::format("Last played: {} (position: {:.1f}s)", 
                    saved_state.current_url, saved_state.position));
                
                // 保存到进度表，下次播放时可以从该位置继续
                DatabaseManager::instance().save_progress(
                    saved_state.current_url, saved_state.position, false);
            }
            
            //恢复UI状态
            ui.set_scroll_mode(saved_state.scroll_mode);
            ui.set_show_tree_lines(saved_state.show_tree_lines);
            EVENT_LOG(fmt::format("Restored UI: scroll_mode={}, tree_lines={}", 
                       saved_state.scroll_mode ? "ON" : "OFF", 
                       saved_state.show_tree_lines ? "ON" : "OFF"));
            
            // 恢复模式
            if (saved_state.current_mode >= 0 && saved_state.current_mode <= 4) {
                mode = static_cast<AppMode>(saved_state.current_mode);
                switch (mode) {
                    case AppMode::RADIO: current_root = radio_root; break;
                    case AppMode::PODCAST: current_root = podcast_root; break;
                    case AppMode::FAVOURITE: current_root = fav_root; break;
                    case AppMode::HISTORY: current_root = history_root; break;
                    case AppMode::ONLINE: 
                        current_root = OnlineState::instance().online_root; 
                        OnlineState::instance().load_search_history();
                        break;
                }
                EVENT_LOG(fmt::format("Restored mode: {}", static_cast<int>(mode)));
            }
            
            // 恢复上次播放的标题
            if (!saved_state.current_title.empty()) {
                EVENT_LOG(fmt::format("Last played: {}", saved_state.current_title));
            }
        }
        
        void load_radio_root() {
            EVENT_LOG("Fetching Radio stations...");
            std::string data = Network::fetch("https://opml.radiotime.com/Browse.ashx?formats=mp3,aac");
            if (!data.empty()) {
                auto parsed = OPMLParser::parse(data);
                if (parsed) {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    radio_root->children = parsed->children;
                    radio_root->children_loaded = true;
                    EVENT_LOG(fmt::format("Radio: {} stations loaded", radio_root->children.size()));
                    Persistence::save_cache(radio_root, podcast_root);
                }
            }
        }
        
        void spawn_load_radio(TreeNodePtr node, bool force = false) {
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                if (node->loading) return;
                node->loading = true;
            }
            
            //统一打印完整URL
            EVENT_LOG(fmt::format("Loading: [RADIO] {}", node->url));
            
            if (node->children_loaded && !force) {
                node->loading = false;
                node->expanded = true;
                return;
            }
            
            std::thread([this, node]() {
                std::string url = node->url;
                //重置XML错误状态
                reset_xml_error_state();
                
                std::string data = Network::fetch(url);
                if (!data.empty()) {
                    auto parsed = OPMLParser::parse(data);
                    if (parsed) {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        node->children = parsed->children;
                        node->children_loaded = true;
                        node->expanded = true;
                        node->parse_failed = false;
                        node->is_cached = true;
                        CacheManager::instance().mark_cached(node->url);
                        EVENT_LOG(fmt::format("Loaded: {} items", node->children.size()));
                        Persistence::save_cache(radio_root, podcast_root);
                    }
                }
                node->loading = false;
                
                //同步更新LINK节点状态
                sync_link_node_status(node);
            }).detach();
        }
        
        void spawn_load_feed(TreeNodePtr node) {
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                if (node->loading) return;
                node->loading = true;
                node->parse_failed = false;
            }
            
            //重置XML错误状态
            reset_xml_error_state();
            
            std::string url = node->url;
            URLType url_type = URLClassifier::classify(url);
            
            //统一打印完整URL
            EVENT_LOG(fmt::format("Loading: [{}] {}", URLClassifier::type_name(url_type), url));
            
            if (url_type == URLType::APPLE_PODCAST) {
                std::string feed = Network::lookup_apple_feed(url);
                if (!feed.empty()) {
                    url = feed;
                    url_type = URLClassifier::classify(url);
                    //打印完整的Apple转换后URL
                    EVENT_LOG(fmt::format("Apple -> {}", url));
                } else {
                    EVENT_LOG("Apple lookup failed");
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    node->loading = false;
                    node->parse_failed = true;
                    node->error_msg = "Apple lookup failed";
                    return;
                }
            }
            
            std::thread([this, node, url, url_type]() {
                //重置XML错误状态（线程内）
                reset_xml_error_state();
                
                TreeNodePtr result = nullptr;
                
                switch (url_type) {
                    case URLType::YOUTUBE_CHANNEL:
                        result = YouTubeChannelParser::parse(url);
                        break;
                    case URLType::YOUTUBE_RSS:
                    case URLType::RSS_PODCAST: {
                        std::string data = Network::fetch(url);
                        if (!data.empty()) result = RSSParser::parse(data, url);
                        break;
                    }
                    case URLType::OPML: {
                        std::string data = Network::fetch(url);
                        if (!data.empty()) {
                            auto parsed = OPMLParser::parse(data);
                            if (parsed) result = parsed;
                        }
                        break;
                    }
                    default: {
                        std::string data = Network::fetch(url);
                        if (!data.empty()) result = RSSParser::parse(data, url);
                        break;
                    }
                }
                
                //在整个更新过程中保持锁，确保loading状态正确同步
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                
                if (result) {
                    if (!result->children.empty()) {
                        node->children = result->children;
                        node->children_loaded = true;
                        node->expanded = true;  // 自动展开以显示加载的内容
                        node->parse_failed = false;
                        node->is_cached = true;
                        if (result->is_youtube) node->is_youtube = true;
                        CacheManager::instance().mark_cached(node->url);
                        EVENT_LOG(fmt::format("Loaded: {} items", result->children.size()));
                        Persistence::save_cache(radio_root, podcast_root);
                        //保存订阅列表到数据库（确保重启后能正确加载）
                        Persistence::save_data(podcast_root->children, fav_root->children);

                        //保存节目列表到数据库缓存
                        // 这样重启后再次展开时不需要重新在线解析
                        json episodes_json = json::array();
                        for (const auto& child : result->children) {
                            json ep;
                            ep["url"] = child->url;
                            ep["title"] = child->title;
                            ep["duration"] = child->duration;
                            ep["is_youtube"] = child->is_youtube;
                            episodes_json.push_back(ep);
                        }
                        DatabaseManager::instance().save_episode_cache(url, episodes_json.dump());
                    } else if (result->parse_failed) {
                        node->parse_failed = true;
                        node->error_msg = result->error_msg;
                        EVENT_LOG(fmt::format("Parse failed: {}", result->error_msg));
                    } else {
                        node->parse_failed = true;
                        node->error_msg = "No items found";
                        EVENT_LOG("No items found in feed");
                    }
                } else {
                    node->parse_failed = true;
                    node->error_msg = "Parser returned null";
                    EVENT_LOG("Parser returned null");
                }
                //在最后设置loading=false，确保状态更新顺序正确
                node->loading = false;
                
                //同步更新LINK节点状态
                // 当target节点加载完成后，同步更新所有引用该节点的LINK节点
                sync_link_node_status(node);
            }).detach();
        }
        
        // V0.05B9n3e5g3d: TOP100 热门播客订阅源 + YouTube频道
        // 内置全球热门播客，用户删除订阅后重启不再自动恢复
        void load_default_podcasts() {
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            struct BuiltinPodcast { const char* name; const char* url; };
            
            // 收集已存在的URL，避免重复添加
            std::set<std::string> existing_urls;
            for (const auto& child : podcast_root->children) {
                if (!child->url.empty()) {
                    existing_urls.insert(child->url);
                }
            }
            
            // TOP200 Apple Podcasts 热门播客 (2026-03-26更新)
            // 数据来源: Apple RSS Marketing Tools API
            // 分类：新闻(News)、喜剧(Comedy)、真实犯罪(True Crime)、商业(Business)、
            //       社会/文化(Society & Culture)、历史(History)、体育(Sports)、
            //       健康(Health & Fitness)、教育(Education)、宗教(Religion)、科技(Technology)
            std::vector<BuiltinPodcast> defaults = {
                // ═══════════════════════════════════════════════════════════════════
                // 【TOP1-20 热门播客】
                // ═══════════════════════════════════════════════════════════════════
                {"Call Her Daddy", "https://podcasts.apple.com/us/podcast/call-her-daddy/id1418960261"},
                {"The Joe Rogan Experience", "https://podcasts.apple.com/us/podcast/the-joe-rogan-experience/id360084272"},
                {"The Rest Is History", "https://podcasts.apple.com/us/podcast/the-rest-is-history/id1537788786"},
                {"Good Hang with Amy Poehler", "https://podcasts.apple.com/us/podcast/good-hang-with-amy-poehler/id1795483480"},
                {"The Mel Robbins Podcast", "https://podcasts.apple.com/us/podcast/the-mel-robbins-podcast/id1646101002"},
                {"The Daily", "https://podcasts.apple.com/us/podcast/the-daily/id1200361736"},
                {"The Diary Of A CEO", "https://podcasts.apple.com/us/podcast/the-diary-of-a-ceo-with-steven-bartlett/id1291423644"},
                {"Crime Junkie", "https://podcasts.apple.com/us/podcast/crime-junkie/id1322200189"},
                {"The Weekly Show with Jon Stewart", "https://podcasts.apple.com/us/podcast/the-weekly-show-with-jon-stewart/id1583132133"},
                {"Dateline NBC", "https://podcasts.apple.com/us/podcast/dateline-nbc/id1464919521"},
                {"The Secret World of Roald Dahl", "https://podcasts.apple.com/us/podcast/the-secret-world-of-roald-dahl/id1868436905"},
                {"48 Hours", "https://podcasts.apple.com/us/podcast/48-hours/id965818306"},
                {"Love Trapped", "https://podcasts.apple.com/us/podcast/love-trapped/id1878220033"},
                {"Pardon My Take", "https://podcasts.apple.com/us/podcast/pardon-my-take/id1089022756"},
                {"Pod Save America", "https://podcasts.apple.com/us/podcast/pod-save-america/id1192761536"},
                {"20/20", "https://podcasts.apple.com/us/podcast/20-20/id987967575"},
                {"Morbid", "https://podcasts.apple.com/us/podcast/morbid/id1379959217"},
                {"The Ezra Klein Show", "https://podcasts.apple.com/us/podcast/the-ezra-klein-show/id1548604447"},
                {"The Bill Simmons Podcast", "https://podcasts.apple.com/us/podcast/the-bill-simmons-podcast/id1043699613"},
                {"SmartLess", "https://podcasts.apple.com/us/podcast/smartless/id1521578868"},
                
                // ═══════════════════════════════════════════════════════════════════
                // 【TOP21-50 新闻/政治 News & Politics】
                // ═══════════════════════════════════════════════════════════════════
                {"This Past Weekend w/ Theo Von", "https://podcasts.apple.com/us/podcast/this-past-weekend-w-theo-von/id1190981360"},
                {"Stuff You Should Know", "https://podcasts.apple.com/us/podcast/stuff-you-should-know/id278981407"},
                {"The Bulwark Podcast", "https://podcasts.apple.com/us/podcast/the-bulwark-podcast/id1447684472"},
                {"The Shawn Ryan Show", "https://podcasts.apple.com/us/podcast/the-shawn-ryan-show/id1492492083"},
                {"The MeidasTouch Podcast", "https://podcasts.apple.com/us/podcast/the-meidastouch-podcast/id1510240831"},
                {"The Toast", "https://podcasts.apple.com/us/podcast/the-toast/id1368081567"},
                {"Trace of Suspicion", "https://podcasts.apple.com/us/podcast/trace-of-suspicion/id1880711858"},
                {"Up First from NPR", "https://podcasts.apple.com/us/podcast/up-first-from-npr/id1222114325"},
                {"Candace", "https://podcasts.apple.com/us/podcast/candace/id1750591415"},
                {"The Megyn Kelly Show", "https://podcasts.apple.com/us/podcast/the-megyn-kelly-show/id1532976305"},
                {"Armchair Expert", "https://podcasts.apple.com/us/podcast/armchair-expert-with-dax-shepard/id1345682353"},
                {"Giggly Squad", "https://podcasts.apple.com/us/podcast/giggly-squad/id1536352412"},
                {"Real Vikings", "https://podcasts.apple.com/us/podcast/real-vikings/id1876255143"},
                {"Betrayal Season 5", "https://podcasts.apple.com/us/podcast/betrayal-season-5/id1615637724"},
                {"My Favorite Murder", "https://podcasts.apple.com/us/podcast/my-favorite-murder/id1074507850"},
                {"The Rest Is Politics", "https://podcasts.apple.com/us/podcast/the-rest-is-politics/id1611374685"},
                {"The Ben Shapiro Show", "https://podcasts.apple.com/us/podcast/the-ben-shapiro-show/id1047335260"},
                {"Bridge of Lies", "https://podcasts.apple.com/us/podcast/bridge-of-lies/id1874641982"},
                {"Global News Podcast", "https://podcasts.apple.com/us/podcast/global-news-podcast/id135067274"},
                {"MrBallen Podcast", "https://podcasts.apple.com/us/podcast/mrballen-podcast/id1608813794"},
                {"The Tucker Carlson Show", "https://podcasts.apple.com/us/podcast/the-tucker-carlson-show/id1719657632"},
                {"Conan O'Brien Needs A Friend", "https://podcasts.apple.com/us/podcast/conan-obrien-needs-a-friend/id1438054347"},
                {"The Rest Is Politics: US", "https://podcasts.apple.com/us/podcast/the-rest-is-politics-us/id1743030473"},
                {"The Bible in a Year", "https://podcasts.apple.com/us/podcast/the-bible-in-a-year/id1539568321"},
                {"The Ramsey Show", "https://podcasts.apple.com/us/podcast/the-ramsey-show/id77001367"},
                {"REAL AF with Andy Frisella", "https://podcasts.apple.com/us/podcast/real-af-with-andy-frisella/id1012570406"},
                {"The Deck", "https://podcasts.apple.com/us/podcast/the-deck/id1603011962"},
                {"Huberman Lab", "https://podcasts.apple.com/us/podcast/huberman-lab/id1545953110"},
                {"The Dan Le Batard Show", "https://podcasts.apple.com/us/podcast/the-dan-le-batard-show/id934820588"},
                {"This American Life", "https://podcasts.apple.com/us/podcast/this-american-life/id201671138"},
                
                // ═══════════════════════════════════════════════════════════════════
                // 【TOP51-100 更多热门】
                // ═══════════════════════════════════════════════════════════════════
                {"Mick Unplugged", "https://podcasts.apple.com/us/podcast/mick-unplugged/id1731755953"},
                {"Matt and Shane's Secret Podcast", "https://podcasts.apple.com/us/podcast/matt-and-shanes-secret-podcast/id1177068388"},
                {"Off Duty | The Guardian", "https://podcasts.apple.com/us/podcast/off-duty/id1731314182"},
                {"The News Agents", "https://podcasts.apple.com/us/podcast/the-news-agents/id1640878689"},
                {"The Louis Theroux Podcast", "https://podcasts.apple.com/us/podcast/the-louis-theroux-podcast/id1725833532"},
                {"The Romesh Ranganathan Show", "https://podcasts.apple.com/us/podcast/the-romesh-ranganathan-show/id1838594107"},
                {"Snapped: Women Who Murder", "https://podcasts.apple.com/us/podcast/snapped-women-who-murder/id1145089790"},
                {"Pivot", "https://podcasts.apple.com/us/podcast/pivot/id1073226719"},
                {"The Rest Is Entertainment", "https://podcasts.apple.com/us/podcast/the-rest-is-entertainment/id1718287198"},
                {"Parenting Hell", "https://podcasts.apple.com/us/podcast/parenting-hell/id1510251497"},
                {"Page 94: The Private Eye Podcast", "https://podcasts.apple.com/us/podcast/page-94/id973958702"},
                {"The Dan Bongino Show", "https://podcasts.apple.com/us/podcast/the-dan-bongino-show/id965293227"},
                {"The Rest Is Science", "https://podcasts.apple.com/us/podcast/the-rest-is-science/id1853007888"},
                {"Newscast", "https://podcasts.apple.com/us/podcast/newscast/id1234185718"},
                {"The Book Club", "https://podcasts.apple.com/us/podcast/the-book-club/id1876049295"},
                {"Off Menu", "https://podcasts.apple.com/us/podcast/off-menu/id1442950743"},
                {"Freakonomics Radio", "https://podcasts.apple.com/us/podcast/freakonomics-radio/id354668519"},
                {"Hanging Out With Ant & Dec", "https://podcasts.apple.com/us/podcast/hanging-out/id1867521360"},
                {"Dish", "https://podcasts.apple.com/us/podcast/dish/id1626354833"},
                {"Morning Wire", "https://podcasts.apple.com/us/podcast/morning-wire/id1576594336"},
                {"Sh**ged Married Annoyed", "https://podcasts.apple.com/us/podcast/sh-ged-married-annoyed/id1451489585"},
                {"No Such Thing As A Fish", "https://podcasts.apple.com/us/podcast/no-such-thing-as-a-fish/id840986946"},
                {"NPR News Now", "https://podcasts.apple.com/us/podcast/npr-news-now/id121493675"},
                {"Foundling | Tortoise", "https://podcasts.apple.com/us/podcast/foundling/id1590561275"},
                {"Elis James and John Robins", "https://podcasts.apple.com/us/podcast/elis-james-john-robins/id1465290044"},
                {"Off Air with Jane & Fi", "https://podcasts.apple.com/us/podcast/off-air/id1648663774"},
                {"The Joe Budden Podcast", "https://podcasts.apple.com/us/podcast/the-joe-budden-podcast/id1535809341"},
                {"Wolf & Owl", "https://podcasts.apple.com/us/podcast/wolf-owl/id1540826523"},
                {"Focus: Adults in the Room", "https://podcasts.apple.com/us/podcast/focus/id1733735613"},
                {"The Idiot", "https://podcasts.apple.com/us/podcast/the-idiot/id1884735227"},
                {"Happy Place", "https://podcasts.apple.com/us/podcast/happy-place/id1353058891"},
                {"The Dylan Gemelli Podcast", "https://podcasts.apple.com/us/podcast/the-dylan-gemelli/id1780873400"},
                {"We Need To Talk", "https://podcasts.apple.com/us/podcast/we-need-to-talk/id1765126946"},
                {"The Viall Files", "https://podcasts.apple.com/us/podcast/the-viall-files/id1448210981"},
                {"Fin vs History", "https://podcasts.apple.com/us/podcast/fin-vs-history/id1790458615"},
                {"My Therapist Ghosted Me", "https://podcasts.apple.com/us/podcast/my-therapist-ghosted-me/id1560176558"},
                {"Last Podcast On The Left", "https://podcasts.apple.com/us/podcast/last-podcast-on-the-left/id437299706"},
                {"Today in Focus", "https://podcasts.apple.com/us/podcast/today-in-focus/id1440133626"},
                {"RedHanded", "https://podcasts.apple.com/us/podcast/redhanded/id1250599915"},
                {"The Rest Is Classified", "https://podcasts.apple.com/us/podcast/the-rest-is-classified/id1780384916"},
                {"Rosebud with Gyles Brandreth", "https://podcasts.apple.com/us/podcast/rosebud/id1704806594"},
                {"Staying Relevant", "https://podcasts.apple.com/us/podcast/staying-relevant/id1651140064"},
                {"Unblinded with Sean Callagy", "https://podcasts.apple.com/us/podcast/unblinded/id1844970260"},
                {"WW2 Pod: We Have Ways", "https://podcasts.apple.com/us/podcast/ww2-pod/id1457552694"},
                {"Park Predators", "https://podcasts.apple.com/us/podcast/park-predators/id1517651197"},
                {"Pod Save the World", "https://podcasts.apple.com/us/podcast/pod-save-the-world/id1200016351"},
                {"Short History Of...", "https://podcasts.apple.com/us/podcast/short-history-of/id1579040306"},
                {"Last One Laughing Podcast", "https://podcasts.apple.com/us/podcast/last-one-laughing/id1885620207"},
                {"Fresh Air", "https://podcasts.apple.com/us/podcast/fresh-air/id214089682"},
                {"Football Weekly", "https://podcasts.apple.com/us/podcast/football-weekly/id188674007"},
                
                // ═══════════════════════════════════════════════════════════════════
                // 【TOP101-150 新闻与商业】
                // ═══════════════════════════════════════════════════════════════════
                {"IHIP News", "https://podcasts.apple.com/us/podcast/ihip-news/id1761444284"},
                {"Empire: World History", "https://podcasts.apple.com/us/podcast/empire/id1639561921"},
                {"LuAnna: The Podcast", "https://podcasts.apple.com/us/podcast/luanna/id1496019465"},
                {"Money Rehab with Nicole Lapin", "https://podcasts.apple.com/us/podcast/money-rehab/id1559564016"},
                {"British Scandal", "https://podcasts.apple.com/us/podcast/british-scandal/id1563775446"},
                {"Dig It with Jo Whiley", "https://podcasts.apple.com/us/podcast/dig-it/id1825368127"},
                {"Alan Carr's Life's a Beach", "https://podcasts.apple.com/us/podcast/lifes-a-beach/id1550998864"},
                {"Habits and Hustle", "https://podcasts.apple.com/us/podcast/habits-and-hustle/id1451897026"},
                {"The Rest Is Football", "https://podcasts.apple.com/us/podcast/the-rest-is-football/id1701022490"},
                {"Young and Profiting", "https://podcasts.apple.com/us/podcast/young-profiting/id1368888880"},
                {"Americast", "https://podcasts.apple.com/us/podcast/americast/id1473150244"},
                {"Proven Podcast", "https://podcasts.apple.com/us/podcast/proven-podcast/id1744386875"},
                {"Stick to Football", "https://podcasts.apple.com/us/podcast/stick-to-football/id1709142395"},
                {"Help I Sexted My Boss", "https://podcasts.apple.com/us/podcast/help-i-sexted/id1357127065"},
                {"Fraudacious", "https://podcasts.apple.com/us/podcast/fraudacious/id1879610796"},
                {"FT News Briefing", "https://podcasts.apple.com/us/podcast/ft-news-briefing/id1438449989"},
                {"The Matt Walsh Show", "https://podcasts.apple.com/us/podcast/the-matt-walsh-show/id1367210511"},
                {"Great Company with Jamie Laing", "https://podcasts.apple.com/us/podcast/great-company/id1735702250"},
                {"Spittin Chiclets", "https://podcasts.apple.com/us/podcast/spittin-chiclets/id1112425552"},
                {"Three Bean Salad", "https://podcasts.apple.com/us/podcast/three-bean-salad/id1564066507"},
                {"Casefile True Crime", "https://podcasts.apple.com/us/podcast/casefile-true-crime/id998568017"},
                {"Raging Moderates", "https://podcasts.apple.com/us/podcast/raging-moderates/id1774505095"},
                {"Front Burner", "https://podcasts.apple.com/us/podcast/front-burner/id1439621628"},
                {"Dan Snow's History Hit", "https://podcasts.apple.com/us/podcast/history-hit/id1042631089"},
                {"Someone Knows Something", "https://podcasts.apple.com/us/podcast/someone-knows-something/id1089216339"},
                {"The Vault Unlocked", "https://podcasts.apple.com/us/podcast/the-vault-unlocked/id1837193185"},
                {"The Archers", "https://podcasts.apple.com/us/podcast/the-archers/id265970428"},
                {"OverDrive", "https://podcasts.apple.com/us/podcast/overdrive/id679367618"},
                {"Desert Island Discs", "https://podcasts.apple.com/us/podcast/desert-island-discs/id342735925"},
                {"All-In Podcast", "https://podcasts.apple.com/us/podcast/all-in/id1502871393"},
                {"The Rewatchables", "https://podcasts.apple.com/us/podcast/the-rewatchables/id1268527882"},
                {"Joe Marler Will See You Now", "https://podcasts.apple.com/us/podcast/joe-marler/id1850736713"},
                {"The Therapy Crouch", "https://podcasts.apple.com/us/podcast/the-therapy-crouch/id1665665408"},
                {"Bad Friends", "https://podcasts.apple.com/us/podcast/bad-friends/id1496265971"},
                {"Feel Better, Live More", "https://podcasts.apple.com/us/podcast/feel-better-live-more/id1333552422"},
                {"Watch What Crappens", "https://podcasts.apple.com/us/podcast/watch-what-crappens/id498130432"},
                {"That Peter Crouch Podcast", "https://podcasts.apple.com/us/podcast/that-peter-crouch/id1616744464"},
                {"The Bridge with Peter Mansbridge", "https://podcasts.apple.com/us/podcast/the-bridge/id1478036186"},
                {"World Report", "https://podcasts.apple.com/us/podcast/world-report/id278657031"},
                {"Chatabix", "https://podcasts.apple.com/us/podcast/chatabix/id1560965008"},
                {"The Cult Queen of Canada", "https://podcasts.apple.com/us/podcast/cult-queen/id1364665348"},
                {"The Rest Is Politics: Leading", "https://podcasts.apple.com/us/podcast/rest-is-politics-leading/id1665265193"},
                {"The Journal", "https://podcasts.apple.com/us/podcast/the-journal/id1469394914"},
                {"Begin Again with Davina McCall", "https://podcasts.apple.com/us/podcast/begin-again/id1773104705"},
                {"The Bible Recap", "https://podcasts.apple.com/us/podcast/the-bible-recap/id1440833267"},
                {"Museum of Pop Culture", "https://podcasts.apple.com/us/podcast/museum-pop-culture/id1863943807"},
                {"Frank Skinner Podcast", "https://podcasts.apple.com/us/podcast/frank-skinner/id308800732"},
                {"Adam Carolla Show", "https://podcasts.apple.com/us/podcast/adam-carolla/id306390087"},
                {"ZOE Science & Nutrition", "https://podcasts.apple.com/us/podcast/zoe-science-nutrition/id1611216298"},
                {"Blair & Barker", "https://podcasts.apple.com/us/podcast/blair-barker/id541972447"},
                
                // ═══════════════════════════════════════════════════════════════════
                // 【TOP151-200 更多热门播客】
                // ═══════════════════════════════════════════════════════════════════
                {"Digital Social Hour", "https://podcasts.apple.com/us/podcast/digital-social-hour/id1676846015"},
                {"The Daily Show: Ears Edition", "https://podcasts.apple.com/us/podcast/the-daily-show/id1334878780"},
                {"Table Manners", "https://podcasts.apple.com/us/podcast/table-manners/id1305228910"},
                {"32 Thoughts: The Podcast", "https://podcasts.apple.com/us/podcast/32-thoughts/id1332150124"},
                {"How To Fail With Elizabeth Day", "https://podcasts.apple.com/us/podcast/how-to-fail/id1407451189"},
                {"The Determined Society", "https://podcasts.apple.com/us/podcast/determined-society/id1555922064"},
                {"Real Kyper & Bourne", "https://podcasts.apple.com/us/podcast/real-kyper-bourne/id1588452517"},
                {"What Did You Do Yesterday?", "https://podcasts.apple.com/us/podcast/what-did-you-do/id1765600990"},
                {"On Purpose with Jay Shetty", "https://podcasts.apple.com/us/podcast/on-purpose/id1450994021"},
                {"Canadian True Crime", "https://podcasts.apple.com/us/podcast/canadian-true-crime/id1197095887"},
                {"The Learning Leader Show", "https://podcasts.apple.com/us/podcast/learning-leader/id985396258"},
                {"ill-advised by Bill Nighy", "https://podcasts.apple.com/us/podcast/ill-advised/id1842190360"},
                {"Nothing much happens", "https://podcasts.apple.com/us/podcast/nothing-much-happens/id1378040733"},
                {"The High Performance Podcast", "https://podcasts.apple.com/us/podcast/high-performance/id1500444735"},
                {"Hidden Brain", "https://podcasts.apple.com/us/podcast/hidden-brain/id1028908750"},
                {"Serial", "https://podcasts.apple.com/us/podcast/serial/id917918570"},
                {"The Daily T", "https://podcasts.apple.com/us/podcast/the-daily-t/id1489612924"},
                {"Get Birding", "https://podcasts.apple.com/us/podcast/get-birding/id1551111133"},
                {"What's My Age Again?", "https://podcasts.apple.com/us/podcast/whats-my-age-again/id1806655079"},
                {"Football Daily", "https://podcasts.apple.com/us/podcast/football-daily/id261291929"},
                {"Mike Ward Sous Écoute", "https://podcasts.apple.com/us/podcast/mike-ward/id1053196322"},
                {"Your World Tonight", "https://podcasts.apple.com/us/podcast/your-world-tonight/id250083757"},
                {"The Jamie Kern Lima Show", "https://podcasts.apple.com/us/podcast/jamie-kern-lima/id1728723635"},
                {"Crime Next Door", "https://podcasts.apple.com/us/podcast/crime-next-door/id1744545000"},
                {"Power & Politics", "https://podcasts.apple.com/us/podcast/power-politics/id337361397"},
                {"Creating Confidence", "https://podcasts.apple.com/us/podcast/creating-confidence/id1462192400"},
                {"Football Ramble", "https://podcasts.apple.com/us/podcast/football-ramble/id254078311"},
                {"The Money Mondays", "https://podcasts.apple.com/us/podcast/the-money-mondays/id1664983297"},
                {"Strangers on a Bench", "https://podcasts.apple.com/us/podcast/strangers-bench/id1770745380"},
                {"Smosh Reads Reddit Stories", "https://podcasts.apple.com/us/podcast/smosh-reads/id1697425543"},
                {"Sword and Scale", "https://podcasts.apple.com/us/podcast/sword-and-scale/id790487079"},
                {"Amanda Knox Hosts | DOUBT", "https://podcasts.apple.com/us/podcast/doubt/id1877870463"},
                {"The Prof G Pod", "https://podcasts.apple.com/us/podcast/prof-g-pod/id1498802610"},
                {"The NPR Politics Podcast", "https://podcasts.apple.com/us/podcast/npr-politics/id1057255460"},
                {"The Ancients", "https://podcasts.apple.com/us/podcast/the-ancients/id1520403988"},
                {"The Basement Yard", "https://podcasts.apple.com/us/podcast/the-basement-yard/id1034354283"},
                {"You're Dead to Me", "https://podcasts.apple.com/us/podcast/youre-dead-to-me/id1479973402"},
                {"Ologies with Alie Ward", "https://podcasts.apple.com/us/podcast/ologies/id1278815517"},
                {"Breaking Points", "https://podcasts.apple.com/us/podcast/breaking-points/id1570045623"},
                {"The David Frum Show", "https://podcasts.apple.com/us/podcast/david-frum-show/id1305908387"},
                {"Modern Wisdom", "https://podcasts.apple.com/us/podcast/modern-wisdom/id1347973549"},
                {"Two Blocks from the White House", "https://podcasts.apple.com/us/podcast/two-blocks/id1866939902"},
                {"Today, Explained", "https://podcasts.apple.com/us/podcast/today-explained/id1346207297"},
                {"Passion Struck", "https://podcasts.apple.com/us/podcast/passion-struck/id1553279283"},
                {"Joe and James Fact Up", "https://podcasts.apple.com/us/podcast/joe-james-fact-up/id1838423659"},
                {"Prof G Markets", "https://podcasts.apple.com/us/podcast/prof-g-markets/id1744631325"},
                {"The Decibel", "https://podcasts.apple.com/us/podcast/the-decibel/id1565410296"},
                {"The Martin Lewis Podcast", "https://podcasts.apple.com/us/podcast/martin-lewis/id520802069"},
                {"3 Takeaways", "https://podcasts.apple.com/us/podcast/3-takeaways/id1526080983"},
                {"The Good, The Bad & The Football", "https://podcasts.apple.com/us/podcast/good-bad-football/id1839425706"},
                {"followHIM", "https://podcasts.apple.com/us/podcast/followhim/id1545433056"},
                {"Behind the Bastards", "https://podcasts.apple.com/us/podcast/behind-the-bastards/id1373812661"},
                
                // ═══════════════════════════════════════════════════════════════════
                // 【YouTube频道】
                // ═══════════════════════════════════════════════════════════════════
                {"56BelowTV (YouTube)", "https://www.youtube.com/@56BelowTV"},
            };
            
            for (auto& p : defaults) {
                // 跳过已存在的播客，避免重复
                if (existing_urls.count(p.url)) continue;
                
                auto node = std::make_shared<TreeNode>();
                node->title = p.name;
                node->url = p.url;
                node->type = NodeType::PODCAST_FEED;
                // 初始显示订阅，但未加载节目清单
                // 进入（展开）时才触发解析
                node->children_loaded = false;
                node->expanded = false;
                node->parent = podcast_root;  //设置父节点指针
                // 标记YouTube频道
                if (std::string(p.url).find("youtube.com") != std::string::npos) {
                    node->is_youtube = true;
                }
                podcast_root->children.push_back(node);
            }
            podcast_root->children_loaded = true;
        }
        
        void flatten(const TreeNodePtr& node, int depth, bool is_last, int parent_idx) {
            int current_idx = display_list.size();
            if (node->title != "Root") {
                display_list.push_back({node, depth, is_last, parent_idx});
            }
            if ((node->type == NodeType::FOLDER || node->type == NodeType::PODCAST_FEED) && 
                (node->expanded || node->title == "Root")) {
                int count = node->children.size();
                // V0.05B9n3e5g3n: 直接按children顺序遍历
                // toggle_sort_order()已经对children进行了排序
                // 这里不需要再根据sort_reversed反向遍历
                for (int i = 0; i < count; ++i) {
                    flatten(node->children[i], depth + (node->title != "Root" ? 1 : 0), 
                           i == count - 1, current_idx);
                }
            }
        }
    };
}

void print_usage() {
    //使用全局常量动态更新版本信息，包含开发时间和邮箱
    std::cout << podradio::APP_NAME << " " << podradio::VERSION << " - Terminal Podcast/Radio Player\n";
    std::cout << "By " << podradio::AUTHOR << " <" << podradio::EMAIL << "> @" << podradio::BUILD_TIME << "\n\n";
    std::cout << "Usage:\n";
    std::cout << "  podradio                  Start the application (TUI mode)\n";
    std::cout << "  podradio -a <url>         Add feed from URL\n";
    std::cout << "  podradio -i <file>        Import OPML subscriptions\n";
    std::cout << "  podradio -e <file>        Export to OPML file\n";
    std::cout << "  podradio -t <time>        Sleep timer (see below)\n";
    std::cout << "  podradio --purge          Clear all cached data\n";
    std::cout << "  podradio -h, -?           Show this help\n\n";
    std::cout << "Sleep Timer Formats (-t):\n";
    std::cout << "  5h, 30m, 90s    With suffix (hours/minutes/seconds)\n";
    std::cout << "  1:30:00         HH:MM:SS format\n";
    std::cout << "  5               Auto: <100 = hours (5 hours)\n";
    std::cout << "  100             Auto: >=100 = minutes (100 min)\n\n";
    std::cout << "Data Paths:\n";
    std::cout << "  Database:   ~/.local/share/podradio/podradio.db (SQLite3)\n";
    std::cout << "  Config:     ~/.config/podradio/config.ini\n";
    std::cout << "  Downloads:  ~/Downloads/PodRadio/\n";
    std::cout << "  Log:        ~/.local/share/podradio/podradio.log\n\n";
    std::cout << "Database Tables:\n";
    std::cout << "  play_history  - 播放历史 (max " << podradio::IniConfig::instance().get_history_max_days() << " days / " 
              << podradio::IniConfig::instance().get_history_max_records() << " records)\n";
    std::cout << "  subscriptions - 播客订阅\n";
    std::cout << "  radio_cache   - 电台目录缓存\n";
    std::cout << "  youtube_cache - YouTube频道缓存\n";
    std::cout << "  url_cache     - URL缓存/下载状态\n";
    std::cout << "  favourites    - 收藏项目\n";
    std::cout << "  search_cache  - iTunes搜索缓存\n";
    std::cout << "  podcast_cache - 播客信息缓存\n";
    std::cout << "  episode_cache - 节目列表缓存\n\n";
    std::cout << "Compile:\n";
    std::cout << "  g++ -std=c++17 podradio.cpp -o podradio \\\n";
    std::cout << "      -I/usr/include/libxml2 \\\n";
    std::cout << "      -lmpv -lncurses -lcurl -lxml2 -lfmt -lpthread -lsqlite3\n";
}

int main(int argc, char* argv[]) {
    int opt;
    std::string import_url, export_file, import_opml_file, sleep_time;
    bool purge = false;
    
    //支持长选项 --purge，以及 -h 和 -?
    static struct option long_options[] = {
        {"purge", no_argument, 0, 'P'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    //-e: 支持 -h 和 -? 显示帮助
    while ((opt = getopt_long(argc, argv, "a:i:e:t:h?", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a':  // 添加订阅URL
                import_url = optarg;
                break;
            case 'i':  // 导入OPML文件
                import_opml_file = optarg;
                break;
            case 'e':
                export_file = optarg;
                break;
            case 't':  // 睡眠定时器
                sleep_time = optarg;
                break;
            case 'h':
            case '?':  //-e: -? 也显示帮助
                print_usage();
                return 0;
            case 'P':  // --purge
                purge = true;
                break;
        }
    }
    
    // V2.39-FF: 处理 --purge 清理缓存
    if (purge) {
        const char* home = std::getenv("HOME");
        if (home) {
            std::string cache_dir = std::string(home) + "/.local/share/podradio";
            std::cout << "Purging cache: " << cache_dir << std::endl;
            try {
                podradio::fs::remove_all(cache_dir);
                std::cout << "Cache cleared successfully." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
        return 0;
    }
    
    curl_global_init(CURL_GLOBAL_ALL);
    xmlInitParser();
    
    //立即设置XML错误处理函数，避免libxml2默认输出到stderr
    // 必须在xmlInitParser()之后、任何XML解析之前设置
    xmlSetGenericErrorFunc(NULL, podradio::xml_error_handler);
    xmlSetStructuredErrorFunc(NULL, podradio::xml_structured_error_handler);
    
    //设置睡眠定时器
    if (!sleep_time.empty()) {
        int seconds = podradio::SleepTimer::parse_time_string(sleep_time);
        if (seconds > 0) {
            podradio::SleepTimer::instance().set_duration(seconds);
            std::cout << "Sleep timer set for " << seconds << " seconds" << std::endl;
        } else {
            std::cerr << "Invalid sleep time format: " << sleep_time << std::endl;
        }
    }
    
    //保存原始终端状态、注册信号处理和atexit清理
    podradio::save_terminal_state();
    podradio::setup_signal_handlers();
    atexit(podradio::tui_cleanup);
    
    //修复变量名冲突，将import_opml_file改为import_opml_path
    if (!import_url.empty() || !export_file.empty() || !import_opml_file.empty()) {
        podradio::App app;
        //命令行模式下也需要加载持久化数据，否则-e导出为空
        app.load_data();
        
        if (!import_url.empty()) {
            std::cout << "Importing: " << import_url << std::endl;
            app.import_feed(import_url);
            std::cout << "Waiting for feed to load..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        if (!import_opml_file.empty()) {
            app.import_opml(import_opml_file);
            std::cout << "Waiting for feeds to load..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        if (!export_file.empty()) {
            std::cout << "Exporting to: " << export_file << std::endl;
            app.export_podcasts(export_file);
        }
    } else {
        podradio::App app;
        app.run();
    }
    
    curl_global_cleanup();
    xmlCleanupParser();
    
    return 0;
}
