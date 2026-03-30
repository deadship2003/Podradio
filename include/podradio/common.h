/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    PODRADIO V0.05B9n3e5g3n                                ║
 * ║                   Terminal Podcast/Radio Player                           ║
 * ║                 Author: Panic <Deadship2003@gmail.com>                    ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * common.h - Foundation header for PodRadio project
 *
 * This header contains all shared type definitions, constants, enums,
 * structs, and forward declarations that every module depends on.
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * Architecture Layers
 * ═════════════════════════════════════════════════════════════════════════════
 *   Presentation: UI class (terminal rendering) / LayoutMetrics (layout management)
 *   Business:     PodRadioApp (main controller) / TreeNode (data model)
 *   Data:         DatabaseManager (SQLite) / CacheManager (cache)
 *   Service:      Network (HTTP) / MPVController (playback)
 *   Utility:      Utils (general) / Logger (logging) / IniConfig (configuration)
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * Data Storage
 * ═════════════════════════════════════════════════════════════════════════════
 *   ~/.local/share/podradio/podradio.db  - SQLite database
 *   ~/.local/share/podradio/podradio.log - Runtime log
 *   ~/.config/podradio/config.ini        - Configuration file
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * Dependencies
 * ═════════════════════════════════════════════════════════════════════════════
 *   libmpv, ncurses, libcurl, libxml2, nlohmann/json, fmt, sqlite3
 *
 * ═════════════════════════════════════════════════════════════════════════════
 * Version History
 * ═════════════════════════════════════════════════════════════════════════════
 *   V0.05B9n3e5g3n - Extended sorting (L-list sorting + linked display), PAUSED state fills siblings, PLAYLIST display optimization (5 lines + alignment)
 *   V0.05B9n3e5g3l - UI optimization: rectangular window border closure fix, playlist delete redundant selection marker
 *   V0.05B9n3e5g3k - SINGLE mode cursor sync fix: INFO area playlist correctly shows current playing item
 *   V0.05B9n3e5g3j - L-list lifecycle optimization: auto-fill empty list, cursor sync, playback indicator
 *   V0.05B9n3e5g3i - L-mode INFO & LOG area playlist display sync fix
 *   V0.05B9n3e5g3e - Terminal output fix: mpv/yt-dlp silent mode to prevent screen corruption
 *   V0.05B9n3e5g3d - F-mode playlist fix + multi-type cache parsing extension
 *   V0.05B9n3e5g3c - Dual playlist mode: default (auto-truncate siblings) + L-mode (manual list priority)
 *   V0.05B9n3e5g3b - Playlist auto-play refactor: podcast episodes auto-collect sibling nodes, L-mode truncated playback
 *   V0.05B9n3e5g3a - Subscription persistence fix, F-mode cache priority, subscription icon display optimization
 *   V0.05B9n3e5g3 - Comment cleanup, code standardization
 *   V0.05B9n3e5g2RF8A21 - YouTube parse cache fix
 *   V0.05B9n3e5g2RF8A20 - YouTube parsing simplification, playlist auto-play
 *   V0.05B9n3e5g2RF8A19 - Playlist keep-open fix
 *   V0.05B9n3e5g2RF8A18 - Window resize UI fix
 *   V0.05B9n3e5g2RF8A11 - List popup refactor, play mode support
 *   V0.05B9n3e5g2RF8A - Database refactor, unified LINK mechanism
 *   V0.05A11 - Smart cache playback, terminal state restore
 *
 */

#ifndef PODRADIO_COMMON_H
#define PODRADIO_COMMON_H

// =========================================================
// Standard Library Includes
// =========================================================
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
#include <cmath>      // for std::isnan / std::isinf
#include <random>
#include <array>
#include <locale.h>
#include <cstdarg>    // va_list for xml_error_handler
#include <signal.h>
#include <sys/stat.h>
#include <termios.h>  // for saving/restoring terminal attributes
#include <sys/ioctl.h> // for getting terminal window size
#include <sys/wait.h>  // for WIFEXITED/WEXITSTATUS macros

// =========================================================
// External Library Includes
// =========================================================
// Conditionally include platform-specific headers to improve portability
// These are required by the full application but may not be available
// in all environments (e.g., CI containers, minimal builds)
#if __has_include(<mpv/client.h>)
    #include <mpv/client.h>
    #define PODRADIO_HAS_MPV 1
#else
    #define PODRADIO_HAS_MPV 0
#endif

#if __has_include(<ncurses.h>)
    #include <ncurses.h>
    #define PODRADIO_HAS_NCURSES 1
#else
    #define PODRADIO_HAS_NCURSES 0
#endif

#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <nlohmann/json.hpp>
#include <sqlite3.h>
// fmt is used throughout the codebase with fmt::format()
// If not available, we provide a minimal fallback
#if __has_include(<fmt/core.h>)
    #include <fmt/core.h>
    #include <fmt/chrono.h>
    #define PODRADIO_HAS_FMT 1
#else
    #define PODRADIO_HAS_FMT 0
    #include <cstdio>
    // Minimal fallback for fmt::format() when libfmt is not available
    namespace podradio::fmt_compat {
        inline std::string format(auto&& fmt_str, auto&&... args) {
            // This is a simplified fallback - proper fmt::format requires libfmt
            return std::string(fmt_str);
        }
    }
    namespace podradio::detail {
        using fmt::format = fmt_compat::format;
    }
#endif


namespace podradio
{
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    // =========================================================
    // Global predefined constants - dynamically updated version info
    // Contains development date/time, displayed in the bottom art banner after the author name
    // Format: Mar 3 2026 18:04:15 (single space between month and day)
    // =========================================================
    constexpr const char* APP_NAME     = "PODRADIO";
    constexpr const char* VERSION      = "V0.05B9n3e5g3n";
    constexpr const char* AUTHOR       = "Panic";
    constexpr const char* EMAIL        = "Deadship2003@gmail.com";
    constexpr const char* BUILD_TIME   = "Mar 27 2026 19:45:00";
    constexpr const char* DATA_DIR     = "/.local/share/podradio";
    constexpr const char* CONFIG_DIR   = "/.config/podradio";
    constexpr const char* DOWNLOAD_DIR = "/Downloads/PodRadio";

    // =========================================================
    // Configuration constants - eliminate magic numbers
    // =========================================================

    // Player configuration constants
    constexpr int MAX_VOLUME       = 100;    // Maximum volume
    constexpr int MIN_VOLUME       = 0;      // Minimum volume
    constexpr int VOLUME_STEP      = 5;      // Volume adjustment step
    constexpr double MIN_SPEED     = 0.2;    // Minimum playback speed
    constexpr double MAX_SPEED     = 3.0;    // Maximum playback speed
    constexpr double DEFAULT_SPEED = 1.0;    // Default playback speed
    constexpr double SPEED_STEP    = 0.1;    // Speed adjustment step

    // Network configuration constants
    constexpr int DEFAULT_NETWORK_TIMEOUT_SEC = 30;   // Default network timeout (seconds)
    constexpr int MAX_REDIRECTS               = 10;   // Maximum redirect count
    constexpr int DOWNLOAD_BUFFER_SIZE        = 8192; // Download buffer size

    // UI configuration constants
    constexpr int MIN_PROGRESS_BAR_WIDTH       = 10;  // Minimum progress bar width
    constexpr int MAX_PROGRESS_BAR_WIDTH       = 30;  // Maximum progress bar width
    constexpr int PROGRESS_BAR_RESERVED_SPACE  = 22;  // Progress bar reserved space (rate + ETA)
    constexpr int DEFAULT_LAYOUT_RATIO_NUMERATOR   = 4;   // Default layout ratio numerator
    constexpr int DEFAULT_LAYOUT_RATIO_DENOMINATOR = 10;  // Default layout ratio denominator
    constexpr int PAGE_SCROLL_LINES            = 10;  // Page scroll lines
    constexpr int DOWNLOAD_COMPLETE_DISPLAY_SEC = 5;  // Download complete display duration (seconds)
    constexpr int MAX_LOG_ENTRIES              = 1024; // Maximum log entries (V0.05B9n3e5g2RF8A: changed from 500 to 1024)
    constexpr int SEARCH_CACHE_MAX             = 100;  // Maximum search cache entries
    constexpr int PODCAST_CACHE_DAYS           = 7;    // Podcast cache expiry days
    constexpr int CACHE_EXPIRE_HOURS           = 168;  // Cache expiry time (hours) - 1 week

    // History configuration moved to INI file
    // constexpr int HISTORY_MAX_DAYS = 30;      // Moved to INI configuration
    // constexpr int HISTORY_MAX_RECORDS = 360;  // Moved to INI configuration

    // =========================================================
    // Status bar art effect modular configuration
    // Users can customize art characters for personalization
    // =========================================================

    // Left art effect structure: ART_OUTER_LEFT + version + ART_OUTER_RIGHT
    constexpr const char* ART_OUTER_LEFT   = "🎵 ♫ ";   // Left-left art characters
    constexpr const char* ART_OUTER_RIGHT  = " ♫ 🎵";   // Left-right art characters
    constexpr const char* ART_AUTHOR_LABEL = "";          // Right author label (empty = hidden)

    // Middle bracket style
    constexpr const char* ART_BRACKET_LEFT  = "[ ";      // Left bracket
    constexpr const char* ART_BRACKET_RIGHT = " ]";      // Right bracket

    // Right art effect structure: ART_RIGHT_PREFIX + author_time + ART_RIGHT_SUFFIX
    constexpr const char* ART_RIGHT_PREFIX = "🎵 ♫ ";   // Right-left art characters
    constexpr const char* ART_RIGHT_SUFFIX = " ♫ 🎵";   // Right-right art characters

    // =========================================================
    // Safe layout constants
    // =========================================================
    constexpr int SAFE_BORDER_MARGIN   = 1;  // Right border safe buffer columns
    constexpr int ICON_FIELD_WIDTH     = 3;  // Icon area fixed width (icon + space; emoji width 2 + space 1 = 3)
    constexpr int EMOJI_LOGICAL_WIDTH  = 2;  // Emoji logical width
    constexpr int ASCII_LOGICAL_WIDTH  = 1;  // ASCII logical width

    // =========================================================
    // Enum definitions
    // =========================================================

    // UI icon style system - dual mode support (ASCII/Emoji)
    // Background: Emoji display width varies across terminals (1 or 2 columns),
    // breaking borders. wcwidth() return value != actual display width.
    // V0.05B9n3e5g3h: Restore ASCII/Emoji toggle functionality (U key to switch)
    enum class IconStyle { ASCII, EMOJI };

    enum class NodeType {
        FOLDER,
        RADIO_STREAM,
        PODCAST_FEED,
        PODCAST_EPISODE
    };

    enum class AppMode {
        RADIO,
        PODCAST,
        FAVOURITE,
        HISTORY,
        ONLINE
    };

    enum class AppState {
        BROWSING,
        LOADING,
        BUFFERING,
        PLAYING,
        PAUSED,
        LIST_MODE
    };

    // Playback mode definitions
    enum class PlayMode {
        SINGLE,  // Single play: stop after current item finishes (default)
        CYCLE,   // List cycle: restart from beginning when list finishes
        RANDOM   // Random play: randomly select next item
    };

    // URL type enumeration
    enum class URLType {
        UNKNOWN,
        OPML,
        RSS_PODCAST,
        YOUTUBE_RSS,
        YOUTUBE_CHANNEL,
        YOUTUBE_VIDEO,
        YOUTUBE_PLAYLIST,
        APPLE_PODCAST,
        RADIO_STREAM,
        VIDEO_FILE
    };

    // Status bar color configuration
    enum class StatusBarColorMode {
        RAINBOW,
        RANDOM,
        TIME_BRIGHTNESS,
        FIXED,
        CUSTOM
    };

    // =========================================================
    // Core type definitions (TreeNode, aliases)
    // =========================================================

    struct TreeNode;
    using TreeNodePtr     = std::shared_ptr<TreeNode>;
    using TreeNodeWeakPtr = std::weak_ptr<TreeNode>;

    struct TreeNode
    {
        std::string title;
        std::string url;
        NodeType type;
        bool expanded          = false;
        bool children_loaded   = false;
        bool loading           = false;
        bool marked            = false;
        bool parse_failed      = false;
        std::string error_msg;
        bool is_youtube        = false;
        std::string channel_name;
        std::string subtext;
        int duration           = 0;
        bool is_cached         = false;
        bool is_downloaded     = false;
        bool is_db_cached      = false;  // Database cache flag (from podcast_cache/episode_cache)
        std::string local_file;
        // V0.02: Playback progress
        double play_position    = 0.0;  // Last playback position (seconds)
        bool play_completed     = false; // Whether fully played
        std::vector<std::shared_ptr<TreeNode>> children;

        // LINK support - reference to target node
        // When is_link=true, expand/display uses linked_node's children
        // Unified LINK mechanism refactor
        bool is_link = false;
        TreeNodeWeakPtr linked_node;      // Using weak_ptr to avoid circular references (runtime reference, may be expired)
        std::string link_target_url;      // Link target URL identifier (for persistence and rebuild)
        std::string source_mode;          // Source mode: "RADIO", "PODCAST", "ONLINE", "FAVOURITE", "HISTORY"

        // Child node sort order
        bool sort_reversed = false;       // true=reverse (new->old), false=normal (old->new)

        // Parent node pointer (for auto-play next track)
        TreeNodeWeakPtr parent;
    };

    // Playlist item structure
    // Extended with node_path for SoftLink
    struct PlaylistItem {
        std::string title;
        std::string url;
        int duration  = 0;
        bool is_video = false;
        std::string node_path;  // Node path (SoftLink reference)
    };

    // Persistent playlist item (for database storage)
    struct SavedPlaylistItem : public PlaylistItem {
        int id         = 0;     // Database ID
        int sort_order = 0;     // Sort order
    };

    // Download progress structure
    struct DownloadProgress {
        std::string id;
        std::string title;
        std::string url;
        std::string status;
        int percent = 0;
        std::string speed;
        int eta_seconds = 0;                                  // Estimated time remaining (seconds)
        int64_t downloaded_bytes = 0;                         // Downloaded bytes
        int64_t total_bytes = 0;                              // Total bytes
        bool active   = true;
        bool complete = false;
        bool failed   = false;
        bool is_youtube = false;
        std::chrono::steady_clock::time_point complete_time;  // Completion timestamp
    };

    // libcurl progress callback data
    struct CurlProgressData {
        std::string dl_id;           // ProgressManager download ID
        std::string title;           // Download title
        int64_t last_bytes = 0;      // Last downloaded bytes
        std::chrono::steady_clock::time_point last_time;  // Last update time
    };

    // Event log entry
    struct LogEntry {
        std::string timestamp;
        std::string message;
    };

    // Display item for UI tree rendering
    struct DisplayItem {
        TreeNodePtr node;
        int depth;
        bool is_last;
        int parent_idx;
    };

    // YouTube video info
    struct YouTubeVideoInfo {
        std::string id;
        std::string title;
        std::string url;
    };

    // YouTube channel cache
    struct YouTubeChannelCache {
        std::string channel_name;
        std::vector<YouTubeVideoInfo> videos;
    };

    // Status bar color configuration
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
    // Forward declarations for all classes
    // =========================================================
    class IconManager;
    class URLClassifier;
    class IniConfig;
    class LayoutMetrics;
    class Logger;
    class Utils;
    class ProgressManager;
    class EventLog;
    class DatabaseManager;
    class CacheManager;
    class ITunesSearch;
    class OnlineState;
    class YouTubeCache;
    class Network;
    class OPMLParser;
    class RSSParser;
    class YouTubeChannelParser;
    class Persistence;
    class StatusBarColorRenderer;
    class MPVController;
    class SleepTimer;
    class UI;
    class App;

    // =========================================================
    // Extern global variable declarations
    // =========================================================

    // Global icon style (default Emoji) - switchable at runtime with U key
    extern IconStyle g_icon_style;

    // XML error handling state
    extern std::string g_last_xml_error;
    extern int g_xml_error_count;

    // Terminal state
    extern struct termios g_original_termios;
    extern bool g_termios_saved;
    extern bool g_ncurses_initialized;
    extern int g_original_lines;
    extern int g_original_cols;

    // Global exit request flag (for SIGINT)
    extern std::atomic<bool> g_exit_requested;

    // =========================================================
    // XML error handler function declarations
    // (Implementations must appear after LOG/EVENT_LOG macro definitions)
    // =========================================================
    void xml_error_handler(void* ctx, const char* msg, ...);
    void xml_structured_error_handler(void* ctx, const xmlError* error);
    void reset_xml_error_state();
    std::string get_last_xml_error();

    // XML property retrieval helper - tries multiple property names
    std::string get_xml_prop_any(xmlNodePtr n, std::vector<std::string> ns);

    // =========================================================
    // Terminal state save/restore function declarations
    // =========================================================

    // Save original terminal attributes
    void save_terminal_state();

    // Restore original terminal attributes
    void restore_terminal_state();

    // Terminal cleanup function (for atexit and signal handling)
    void tui_cleanup();

    // =========================================================
    // Signal handler function declarations
    // =========================================================

    // Signal handler (SIGINT sets flag, others clean up and re-raise)
    void signal_handler(int sig);

    // Register signal handlers for SIGINT, SIGTERM, SIGQUIT, SIGSEGV, SIGABRT
    void setup_signal_handlers();

} // namespace podradio


#endif // PODRADIO_COMMON_H
