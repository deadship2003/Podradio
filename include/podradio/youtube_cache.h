#pragma once

#include "common.h"

namespace podradio {

// =========================================================
// YouTube Cache - Caches YouTube channel video lists
// Singleton pattern. Uses in-memory map with database persistence.
//
// NOTE: load_from_db() and save_to_db() currently use direct
// sqlite3 calls with a mutex wrapper. These should be migrated
// to use DatabaseManager::save_youtube_cache() and
// DatabaseManager::load_youtube_cache() once those methods
// are added to DatabaseManager.
// =========================================================
class YouTubeCache {
public:
    static YouTubeCache& instance();

    // Initialize memory cache (database is managed by DatabaseManager)
    void load();

    // Save to database (handled by update() directly)
    void save();

    // Check if channel URL is cached (memory first, then database)
    bool has(const std::string& url);

    // Get cached channel data (memory first, then database)
    YouTubeChannelCache get(const std::string& url);

    // Update cache with new channel data and persist to database
    void update(const std::string& url, const std::string& name,
                const std::vector<YouTubeVideoInfo>& videos);

private:
    YouTubeCache() = default;

    std::map<std::string, YouTubeChannelCache> cache_;
    std::mutex db_mutex_;  // Mutex for direct sqlite3 calls

    // Load YouTube cache from database
    // TODO: Migrate to DatabaseManager::load_youtube_cache()
    bool load_from_db(const std::string& channel_url);

    // Save YouTube cache to database
    // TODO: Migrate to DatabaseManager::save_youtube_cache()
    void save_to_db(const std::string& channel_url, const std::string& channel_name,
                    const std::vector<YouTubeVideoInfo>& videos);
};

} // namespace podradio
