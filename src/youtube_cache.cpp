#include "podradio/youtube_cache.h"
#include "podradio/database.h"

namespace podradio
{

YouTubeCache& YouTubeCache::instance() {
    static YouTubeCache yc;
    return yc;
}

void YouTubeCache::load() {
    // YouTube cache now loads from database youtube_cache table
    // Managed by DatabaseManager, this is memory cache initialization
    cache_.clear();
}

void YouTubeCache::save() {
    // Save to database youtube_cache table
    // Handled by update() method which saves to database directly
}

bool YouTubeCache::has(const std::string& url) {
    // Check memory cache first, then database
    if (cache_.count(url) > 0) return true;
    return load_from_db(url);
}

YouTubeChannelCache YouTubeCache::get(const std::string& url) {
    if (cache_.count(url)) return cache_[url];
    // Try loading from database
    if (load_from_db(url) && cache_.count(url)) {
        return cache_[url];
    }
    return YouTubeChannelCache();
}

void YouTubeCache::update(const std::string& url, const std::string& name,
                          const std::vector<YouTubeVideoInfo>& videos) {
    cache_[url] = {name, videos};
    // Save to database
    save_to_db(url, name, videos);
}

bool YouTubeCache::load_from_db(const std::string& channel_url) {
    // TODO: This should use DatabaseManager::load_youtube_cache() once available.
    // Currently using direct sqlite3 calls wrapped in a mutex for thread safety.
    if (!DatabaseManager::instance().is_ready()) return false;

    const char* home = std::getenv("HOME");
    if (!home) return false;

    std::string db_path = std::string(home) + DATA_DIR + "/podradio.db";
    sqlite3* db = nullptr;

    std::lock_guard<std::mutex> lock(db_mutex_);

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

void YouTubeCache::save_to_db(const std::string& channel_url, const std::string& channel_name,
                              const std::vector<YouTubeVideoInfo>& videos) {
    // TODO: This should use DatabaseManager::save_youtube_cache() once available.
    // Currently using direct sqlite3 calls wrapped in a mutex for thread safety.
    if (!DatabaseManager::instance().is_ready()) return;

    const char* home = std::getenv("HOME");
    if (!home) return;

    std::string db_path = std::string(home) + DATA_DIR + "/podradio.db";
    sqlite3* db = nullptr;

    std::lock_guard<std::mutex> lock(db_mutex_);

    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return;
    }

    // Build videos JSON
    json videos_json = json::array();
    for (const auto& vi : videos) {
        videos_json.push_back({
            {"id", vi.id},
            {"title", vi.title},
            {"url", vi.url}
        });
    }

    // Escape SQL strings
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

} // namespace podradio
