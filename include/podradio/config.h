#pragma once

#include "common.h"

namespace podradio
{

    // =========================================================
    // IniConfig singleton - INI file configuration management
    //
    // Loads/saves configuration from ~/.config/podradio/config.ini
    // Provides typed getters (string, int, float, bool) and
    // convenience methods for specific configuration values.
    // =========================================================
    class IniConfig {
    public:
        static IniConfig& instance();

        // Load configuration from INI file; create default if missing
        void load();

        // Generic getters
        std::string get(const std::string& section, const std::string& key,
                        const std::string& default_val = "") const;
        int get_int(const std::string& section, const std::string& key,
                    int default_val = 0) const;
        float get_float(const std::string& section, const std::string& key,
                        float default_val = 0.0f) const;
        bool get_bool(const std::string& section, const std::string& key,
                      bool default_val = false) const;

        // Typed convenience getters
        StatusBarColorConfig get_statusbar_color_config();
        int get_search_cache_max();
        int get_podcast_cache_days();
        int get_history_max_records();
        int get_history_max_days();
        std::string get_default_region();
        int get_cache_max() const;
        int get_max_log_entries() const;
        int get_cache_expire_hours() const;
        bool get_debug() const;

        // NEW: Network SSL verification settings
        bool get_ssl_verify() const;       // Default: false (skip SSL verification)
        bool get_ssl_verify_feed() const;  // Default: true (verify SSL for feed URLs)

        // Get config file path
        static std::string get_config_file();

    private:
        IniConfig() {}
        IniConfig(const IniConfig&) = delete;
        IniConfig& operator=(const IniConfig&) = delete;

        std::map<std::string, std::map<std::string, std::string>> data_;

        // Create a default configuration file at the given path
        void create_default(const std::string& path);
    };

} // namespace podradio
