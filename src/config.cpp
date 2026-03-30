#include "podradio/config.h"

namespace podradio
{

    IniConfig& IniConfig::instance() {
        static IniConfig ic;
        return ic;
    }

    void IniConfig::load() {
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

    std::string IniConfig::get(const std::string& section, const std::string& key,
                               const std::string& default_val) const {
        if (data_.count(section) && data_.at(section).count(key)) {
            return data_.at(section).at(key);
        }
        return default_val;
    }

    int IniConfig::get_int(const std::string& section, const std::string& key,
                           int default_val) const {
        std::string val = get(section, key);
        if (!val.empty()) {
            try { return std::stoi(val); } catch (...) {}
        }
        return default_val;
    }

    float IniConfig::get_float(const std::string& section, const std::string& key,
                               float default_val) const {
        std::string val = get(section, key);
        if (!val.empty()) {
            try { return std::stof(val); } catch (...) {}
        }
        return default_val;
    }

    bool IniConfig::get_bool(const std::string& section, const std::string& key,
                             bool default_val) const {
        std::string val = get(section, key);
        if (val == "true" || val == "yes" || val == "1") return true;
        if (val == "false" || val == "no" || val == "0") return false;
        return default_val;
    }

    StatusBarColorConfig IniConfig::get_statusbar_color_config() {
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

    // Get max search cache entries (default 1024)
    int IniConfig::get_search_cache_max() {
        return get_int("storage", "search_cache_max", 1024);
    }

    // Get podcast cache expiry days
    int IniConfig::get_podcast_cache_days() {
        return get_int("storage", "podcast_cache_days", 1024);
    }

    // Get max playback history records (default 2048)
    int IniConfig::get_history_max_records() {
        return get_int("storage", "history_max_records", 2048);
    }

    // Get max playback history days (default 1080 days ~ 3 years)
    int IniConfig::get_history_max_days() {
        return get_int("storage", "history_max_days", 1080);
    }

    // Get default search region
    std::string IniConfig::get_default_region() {
        return get("search", "default_region", "US");
    }

    // Additional configuration getters
    int IniConfig::get_cache_max() const { return get_int("general", "cache_max", 1024); }
    int IniConfig::get_max_log_entries() const { return get_int("general", "max_log_entries", 2048); }
    int IniConfig::get_cache_expire_hours() const { return get_int("advanced", "cache_expire_hours", 24576); }
    bool IniConfig::get_debug() const { return get_bool("advanced", "debug", false); }

    // NEW: Network SSL verification settings
    bool IniConfig::get_ssl_verify() const {
        return get_bool("network", "ssl_verify", false);
    }

    bool IniConfig::get_ssl_verify_feed() const {
        return get_bool("network", "ssl_verify_feed", true);
    }

    std::string IniConfig::get_config_file() {
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + CONFIG_DIR + "/config.ini" : "";
    }

    void IniConfig::create_default(const std::string& path) {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream f(path);
        if (f.is_open()) {
            f << R"(# ============================================================
# PODRADIO Configuration File
# Version: )" << VERSION << R"(
# Author: Panic
# ============================================================
#
# This configuration file is auto-generated by the program.
# Restart the program after making changes.
#
# ============================================================
# [Display Settings]
# ============================================================
[display]
# Left/right panel ratio (0.2-0.8)
layout_ratio = 0.4
# Show tree connector lines
show_tree_lines = true
# Enable title scrolling display
scroll_mode = false

# ============================================================
# [Key Bindings]
# ============================================================
[keybindings]
# ===== Navigation =====
up = k              # Move cursor up
down = j            # Move cursor down
quit = q            # Quit program
help = ?            # Show help
page_up = PageUp    # Page up
page_down = PageDown # Page down
go_top = g          # Jump to top
go_bottom = G       # Jump to bottom
back = h            # Go back / collapse
expand = l          # Expand / play (same as Enter)

# ===== Playback Control =====
pause = space       # Pause / resume
vol_up = +          # Volume up
vol_down = -        # Volume down
speed_up = ]        # Increase playback speed
speed_down = [      # Decrease playback speed
speed_reset = \     # Reset speed to 1.0x

# ===== Subscription Management =====
add_feed = a        # Add feed subscription
delete = d          # Delete node
edit = e            # Edit node title / URL
favourite = f       # Toggle favourite
refresh = r         # Reload current node
download = D        # Download item
batch_download = B  # Batch download marked items

# ===== Marking =====
mark = m            # Toggle mark
visual = v          # Enter visual mode
clear_marks = V     # Clear all marks

# ===== Search =====
search = /          # Start search (ESC to cancel)
next_match = J      # Next match
prev_match = K      # Previous match

# ===== UI Control =====
toggle_theme = Ctrl+L  # Toggle dark/light theme (Ctrl+L, ASCII code 12)
toggle_tree = T        # Toggle tree line display
toggle_scroll = S      # Toggle scroll mode

# ===== Mode Switching =====
mode_radio = R      # Switch to RADIO mode
mode_podcast = P    # Switch to PODCAST mode
mode_favourite = F  # Switch to FAVOURITE mode
mode_history = H    # Switch to HISTORY mode
mode_online = O     # Switch to ONLINE mode
mode_cycle = M      # Cycle through all modes

# ===== Online Mode Specific =====
online_search = /   # Enter search keywords
switch_region = B   # Switch search region

# ============================================================
# [Network Settings]
# ============================================================
[network]
# Network request timeout (seconds)
timeout = 30
# Verify SSL certificates for general HTTP requests
# true = verify (secure), false = skip verification (may work with self-signed certs)
ssl_verify = false
# Verify SSL certificates for feed (RSS/OPML) URLs
# true = verify (secure), false = skip verification
ssl_verify_feed = true

# ============================================================
# [Storage Settings]
# ============================================================
[storage]
# Download file save directory
download_dir = ~/Downloads/PodRadio
# EVENT LOG max display entries
max_log_entries = 500
# Log file retention max days (default 1024 days ~ 3 years)
log_max_days = 1024
# Search cache max entries (cleaned by count, not by time)
search_cache_max = 1024
# Playback history max records
history_max_records = 2048
# Playback history max days (~3 years, 0 = unlimited)
history_max_days = 1080
# Podcast info cache expiry days
podcast_cache_days = 1024

# ============================================================
# [YouTube / Video Download Settings]
# ============================================================
[youtube]
# Max video resolution height (360/480/720/1080/1440/2160)
video_max_height = 1080
# Max video frame rate (24/30/60)
video_max_fps = 30
# Download mode:
#   merged  - Merge video+audio into single file (recommended)
#   video   - Download video stream only
#   audio   - Download audio stream only (MP3 format)
download_mode = merged
# Audio download format: mp3/m4a/opus/ogg
audio_format = mp3
# Audio download quality: 0 (best) - 9 (worst), recommended 0
audio_quality = 0
# Embed thumbnail
embed_thumbnail = true
# Embed subtitle
embed_subtitle = false

# ============================================================
# [Status Bar Color Settings]
# ============================================================
[statusbar_color]
# Color mode:
#   rainbow  - Rainbow gradient effect
#   random   - Random color
#   fixed    - Fixed color (use fixed_color)
#   custom   - Custom color sequence
mode = rainbow
# Update interval (milliseconds)
update_interval_ms = 100
# Brightness range (0.0-1.0)
brightness_min = 0.5
brightness_max = 1.0
# Time adjustment effect
time_adjust = true
# Fixed color mode color:
#   black red green yellow blue magenta cyan white
fixed_color = cyan
# Rainbow mode speed (1-10)
rainbow_speed = 1

# ============================================================
# [Advanced Settings]
# ============================================================
[advanced]
# Cache expiry time (hours)
cache_expire_hours = 24
# Debug mode
debug = false

# ============================================================
# [iTunes Search Settings]
# ============================================================
[search]
# Search result cache count (stored in SQLite database)
cache_max = 100
# Default search region:
#   US - United States
#   CN - China
#   TW - Taiwan
#   JP - Japan
#   UK - United Kingdom
#   DE - Germany
#   FR - France
#   KR - Korea
#   AU - Australia
default_region = US

# ============================================================
# [Art Display Color Code Reference]
# ============================================================
# ncurses standard color codes:
#   0 - Default color
#   1 - Black
#   2 - Red
#   3 - Green
#   4 - Yellow
#   5 - Blue
#   6 - Magenta
#   7 - Cyan
#   8 - White
#
# Status bar color mode descriptions:
#   rainbow  - Auto-cycle through all colors for rainbow effect
#   random   - Random color on each update
#   fixed    - Use the single color specified by fixed_color
#   custom   - Use custom color sequence (requires source modification)
# ============================================================
)";
        }
    }

} // namespace podradio
