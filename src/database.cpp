/*
 * database.cpp - DatabaseManager implementation
 *
 * Full implementation of the SQLite persistence layer.
 * Security: all mutations that include user-supplied strings use
 * prepared statements (sqlite3_bind_text) to prevent SQL injection.
 */

#include "podradio/database.h"
#include "podradio/logger.h"
#include "podradio/event_log.h"

#include <cstdlib>
#include <sstream>
#include <algorithm>

// Forward-declare dependencies that live in other modules.
// These are resolved at link time; we only need the types here.
namespace podradio
{
    // IniConfig is declared in common.h forward-declarations and
    // implemented in its own module.  We only call its static methods.
    class IniConfig;

    // LOG / EVENT_LOG are macros defined by the Logger/EventLog modules.
    // They are available once the respective headers are included at the
    // compilation unit that pulls in this .cpp (or via the PCH).
} // namespace podradio

// These macros are expected to be defined by the build system / PCH.
// Provide fallback no-ops so this file compiles standalone.
#ifndef LOG
#define LOG(msg) (void)(msg)
#endif
#ifndef EVENT_LOG
#define EVENT_LOG(msg) (void)(msg)
#endif

namespace podradio
{

// ====================================================================
// Lifecycle
// ====================================================================

bool DatabaseManager::init()
{
    // Singleton guard - prevent double initialisation
    if (initialized_) return true;

    const char* home = std::getenv("HOME");
    if (!home) return false;

    std::string db_path = std::string(home) + DATA_DIR + "/podradio.db";
    fs::create_directories(fs::path(db_path).parent_path());

    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        LOG(fmt::format("[DB] Failed to open: {}", sqlite3_errmsg(db_)));
        return false;
    }

    // Create all tables
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

    // Auto-cleanup expired history on startup
    cleanup_old_history();

    return true;
}

DatabaseManager::~DatabaseManager()
{
    if (db_) sqlite3_close(db_);
}

bool DatabaseManager::is_ready() const
{
    return initialized_ && db_ != nullptr;
}

// ====================================================================
// Low-level helpers
// ====================================================================

[[deprecated("Use prepared statements for user-supplied data")]]
std::string DatabaseManager::escape_sql(const std::string& s)
{
    std::string result;
    result.reserve(s.size() * 2);
    for (char c : s) {
        if (c == '\'') result += "''";
        else result += c;
    }
    return result;
}

bool DatabaseManager::exec_sql(const std::string& sql)
{
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        LOG(fmt::format("[DB] SQL error: {}", err_msg ? err_msg : "unknown"));
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool DatabaseManager::exec_sql_raw(const std::string& sql)
{
    // Same as exec_sql but used for static SQL strings (no user data).
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        LOG(fmt::format("[DB] SQL error: {}", err_msg ? err_msg : "unknown"));
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    return true;
}

// ====================================================================
// Progress management
// ====================================================================

void DatabaseManager::prepared_save_progress(const std::string& url, double pos, bool completed)
{
    const char* sql =
        "INSERT OR REPLACE INTO progress (url, position, completed, last_played) "
        "VALUES (?, ?, ?, datetime('now'));";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, pos);
    sqlite3_bind_int(stmt, 3, completed ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::save_progress(const std::string& url, double position, bool completed)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_save_progress(url, position, completed);
}

std::pair<double, bool> DatabaseManager::get_progress(const std::string& url)
{
    if (!is_ready()) return {0.0, false};
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT position, completed FROM progress WHERE url = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return {0.0, false};
    }
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

    double pos = 0.0;
    bool completed = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        pos = sqlite3_column_double(stmt, 0);
        completed = sqlite3_column_int(stmt, 1) != 0;
    }
    sqlite3_finalize(stmt);
    return {pos, completed};
}

// ====================================================================
// History
// ====================================================================

void DatabaseManager::prepared_add_history(const std::string& url,
                                           const std::string& title,
                                           int duration)
{
    const char* sql =
        "INSERT INTO history (url, title, duration, timestamp) "
        "VALUES (?, ?, ?, datetime('now'));";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, duration);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::add_history(const std::string& url, const std::string& title, int duration)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_add_history(url, title, duration);
}

std::vector<std::tuple<std::string, std::string, std::string>>
DatabaseManager::get_history(int limit)
{
    if (!is_ready()) return {};
    std::lock_guard<std::mutex> lock(mtx_);

    std::vector<std::tuple<std::string, std::string, std::string>> result;
    const char* sql = "SELECT url, title, timestamp FROM history ORDER BY timestamp DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* col_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* col_title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* col_ts   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        result.push_back({
            col_url   ? col_url   : "",
            col_title ? col_title : "",
            col_ts    ? col_ts    : ""
        });
    }
    sqlite3_finalize(stmt);
    return result;
}

void DatabaseManager::prepared_delete_history(const std::string& url)
{
    const char* sql = "DELETE FROM history WHERE url = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::delete_history(const std::string& url)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_delete_history(url);
    LOG(fmt::format("[DB] Deleted history: {}", url));
}

// ====================================================================
// Statistics
// ====================================================================

void DatabaseManager::prepared_increment_stat(const std::string& key, int64_t delta)
{
    const char* sql =
        "INSERT INTO stats (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = CAST(CAST(value AS INTEGER) + ? AS TEXT);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    std::string delta_str = std::to_string(delta);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, delta_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, delta);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::increment_stat(const std::string& key, int64_t delta)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_increment_stat(key, delta);
}

int64_t DatabaseManager::get_stat(const std::string& key, int64_t default_val)
{
    if (!is_ready()) return default_val;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT value FROM stats WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return default_val;
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    int64_t result = default_val;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (val) {
            try { result = std::stoll(val); } catch (...) { result = default_val; }
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

// ====================================================================
// Generic URL cache
// ====================================================================

void DatabaseManager::prepared_cache_url_data(const std::string& url, const std::string& json_data)
{
    const char* sql =
        "INSERT OR REPLACE INTO cache (url, data_json, timestamp) "
        "VALUES (?, ?, strftime('%s', 'now'));";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, json_data.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::cache_url_data(const std::string& url, const std::string& json_data)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_cache_url_data(url, json_data);
}

std::string DatabaseManager::get_cached_data(const std::string& url)
{
    if (!is_ready()) return "";
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT data_json FROM cache WHERE url = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";

    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        result = data ? data : "";
    }
    sqlite3_finalize(stmt);
    return result;
}

// ====================================================================
// Nodes (subscriptions)
// ====================================================================

void DatabaseManager::prepared_save_node(const std::string& url, const std::string& title,
                                         int type, const std::string& json_data)
{
    const char* sql =
        "INSERT OR REPLACE INTO nodes (url, title, type, parent_id, data_json) "
        "VALUES (?, ?, ?, 0, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, type);
    sqlite3_bind_text(stmt, 4, json_data.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::save_node(const std::string& url, const std::string& title,
                                int type, const std::string& json_data)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_save_node(url, title, type, json_data);
}

std::vector<std::tuple<std::string, std::string, int, std::string>>
DatabaseManager::load_nodes()
{
    std::vector<std::tuple<std::string, std::string, int, std::string>> nodes;
    if (!is_ready()) return nodes;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql =
        "SELECT title, url, type, data_json FROM nodes WHERE parent_id = 0 "
        "ORDER BY updated_at DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return nodes;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* title     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* url       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        int type              = sqlite3_column_int(stmt, 2);
        const char* data_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        nodes.push_back({
            title     ? title     : "",
            url       ? url       : "",
            type,
            data_json ? data_json : ""
        });
    }
    sqlite3_finalize(stmt);
    return nodes;
}

void DatabaseManager::clear_nodes()
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    exec_sql("DELETE FROM nodes;");
    LOG("[DB] Cleared nodes table");
}

// ====================================================================
// Purge all tables
// ====================================================================

void DatabaseManager::purge_all()
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    sqlite3_exec(db_,
        "DELETE FROM nodes;"
        "DELETE FROM progress;"
        "DELETE FROM history;"
        "DELETE FROM stats;"
        "DELETE FROM cache;"
        "DELETE FROM search_cache;"
        "DELETE FROM player_state;"
        "DELETE FROM podcast_cache;"
        "DELETE FROM episode_cache;"
        "DELETE FROM favourites;"
        "DELETE FROM radio_cache;"
        "DELETE FROM youtube_cache;"
        "DELETE FROM url_cache;",
        nullptr, nullptr, nullptr);
}

// ====================================================================
// Search cache
// ====================================================================

void DatabaseManager::prepared_save_search_cache(const std::string& query,
                                                  const std::string& region,
                                                  const std::string& result_json,
                                                  bool exists)
{
    sqlite3_stmt* stmt = nullptr;
    if (exists) {
        const char* sql =
            "UPDATE search_cache SET result_json=?, timestamp=CURRENT_TIMESTAMP "
            "WHERE query=? AND region=?;";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        sqlite3_bind_text(stmt, 1, result_json.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, query.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, region.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        const char* sql =
            "INSERT INTO search_cache (query, region, result_json) VALUES (?, ?, ?);";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
        sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, region.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, result_json.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::save_search_cache(const std::string& query,
                                        const std::string& region,
                                        const std::string& result_json)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);

    // Check if a record with the same query+region already exists
    const char* check_sql = "SELECT 1 FROM search_cache WHERE query=? AND region=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    bool exists = false;
    if (sqlite3_prepare_v2(db_, check_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, region.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) exists = true;
        sqlite3_finalize(stmt);
    }

    prepared_save_search_cache(query, region, result_json, exists);

    // Enforce max cache size (numeric limit, safe to format)
    int max_cache = 1024;
    // Try to read from IniConfig if available at link time
    // (default 1024 matches the original default)
    std::string cleanup = fmt::format(
        "DELETE FROM search_cache WHERE id NOT IN "
        "(SELECT id FROM search_cache ORDER BY timestamp DESC LIMIT {});",
        max_cache
    );
    exec_sql(cleanup);
}

std::vector<SearchHistoryItem> DatabaseManager::load_all_search_history()
{
    std::vector<SearchHistoryItem> history;
    if (!is_ready()) return history;
    std::lock_guard<std::mutex> lock(mtx_);

    // Most recent search first
    const char* sql =
        "SELECT id, query, region, timestamp FROM search_cache "
        "ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return history;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SearchHistoryItem item;
        item.id = sqlite3_column_int(stmt, 0);

        const char* q = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        item.query = q ? q : "";

        const char* r = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        item.region = r ? r : "US";

        const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        item.timestamp = t ? t : "";

        // Count results by parsing stored JSON
        const char* json_sql = "SELECT result_json FROM search_cache WHERE id=?;";
        sqlite3_stmt* json_stmt = nullptr;
        if (sqlite3_prepare_v2(db_, json_sql, -1, &json_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(json_stmt, 1, item.id);
            if (sqlite3_step(json_stmt) == SQLITE_ROW) {
                const char* json_data = reinterpret_cast<const char*>(
                    sqlite3_column_text(json_stmt, 0));
                if (json_data) {
                    try {
                        json j = json::parse(json_data);
                        if (j.contains("results") && j["results"].is_array()) {
                            item.result_count = static_cast<int>(j["results"].size());
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
    return history;
}

std::string DatabaseManager::load_search_cache(const std::string& query,
                                                const std::string& region)
{
    if (!is_ready()) return "";
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql =
        "SELECT result_json FROM search_cache "
        "WHERE query=? AND region=? ORDER BY timestamp DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";

    sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, region.c_str(), -1, SQLITE_TRANSIENT);

    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        result = data ? data : "";
    }
    sqlite3_finalize(stmt);
    return result;
}

void DatabaseManager::prepared_delete_search_history(const std::string& query,
                                                      const std::string& region)
{
    const char* sql = "DELETE FROM search_cache WHERE query=? AND region=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, region.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::delete_search_history(const std::string& query,
                                             const std::string& region)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_delete_search_history(query, region);
    LOG(fmt::format("[DB] Deleted search history: {} [{}]", query, region));
}

// ====================================================================
// Favourites
// ====================================================================

void DatabaseManager::prepared_save_favourite(const std::string& title,
                                               const std::string& url,
                                               int type,
                                               bool is_youtube,
                                               const std::string& channel_name,
                                               const std::string& source_type,
                                               const std::string& data_json)
{
    const char* sql =
        "INSERT OR REPLACE INTO favourites "
        "(title, url, type, is_youtube, channel_name, source_type, data_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, type);
    sqlite3_bind_int(stmt, 4, is_youtube ? 1 : 0);
    sqlite3_bind_text(stmt, 5, channel_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, source_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, data_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::save_favourite(const std::string& title, const std::string& url, int type,
                                      bool is_youtube, const std::string& channel_name,
                                      const std::string& source_type, const std::string& data_json)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_save_favourite(title, url, type, is_youtube, channel_name,
                            source_type, data_json);
}

std::vector<std::tuple<std::string, std::string, int, bool, std::string, std::string, std::string>>
DatabaseManager::load_favourites()
{
    std::vector<std::tuple<std::string, std::string, int, bool,
                           std::string, std::string, std::string>> favs;
    if (!is_ready()) return favs;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql =
        "SELECT title, url, type, is_youtube, channel_name, source_type, data_json "
        "FROM favourites ORDER BY created_at DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return favs;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* title        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* url          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        int type                 = sqlite3_column_int(stmt, 2);
        bool is_youtube          = sqlite3_column_int(stmt, 3) != 0;
        const char* channel_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const char* source_type  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        const char* data_json    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

        favs.push_back({
            title        ? title        : "",
            url          ? url          : "",
            type,
            is_youtube,
            channel_name ? channel_name : "",
            source_type  ? source_type  : "",
            data_json    ? data_json    : ""
        });
    }
    sqlite3_finalize(stmt);
    return favs;
}

void DatabaseManager::prepared_delete_favourite(const std::string& url)
{
    const char* sql = "DELETE FROM favourites WHERE url=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::delete_favourite(const std::string& url)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_delete_favourite(url);
}

bool DatabaseManager::is_favourite(const std::string& url)
{
    if (!is_ready()) return false;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT 1 FROM favourites WHERE url=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    bool result = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) result = true;
        sqlite3_finalize(stmt);
    }
    return result;
}

void DatabaseManager::clear_favourites()
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    exec_sql("DELETE FROM favourites;");
    LOG("[DB] Cleared favourites table");
}

// ====================================================================
// Podcast feed cache
// ====================================================================

void DatabaseManager::prepared_save_podcast_cache(const std::string& feed_url,
                                                   const std::string& title,
                                                   const std::string& artist,
                                                   const std::string& genre,
                                                   int track_count,
                                                   const std::string& artwork_url,
                                                   int collection_id,
                                                   const std::string& data_json)
{
    const char* sql =
        "INSERT OR REPLACE INTO podcast_cache "
        "(feed_url, title, artist, genre, track_count, artwork_url, collection_id, data_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, artist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, genre.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, track_count);
    sqlite3_bind_text(stmt, 6, artwork_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, collection_id);
    sqlite3_bind_text(stmt, 8, data_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::save_podcast_cache(const std::string& feed_url, const std::string& title,
                                          const std::string& artist, const std::string& genre,
                                          int track_count, const std::string& artwork_url,
                                          int collection_id, const std::string& data_json)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_save_podcast_cache(feed_url, title, artist, genre,
                                track_count, artwork_url, collection_id, data_json);
}

bool DatabaseManager::is_podcast_cached(const std::string& feed_url)
{
    if (!is_ready()) return false;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT 1 FROM podcast_cache WHERE feed_url=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    bool found = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) found = true;
        sqlite3_finalize(stmt);
    }
    return found;
}

std::string DatabaseManager::load_podcast_cache(const std::string& feed_url)
{
    if (!is_ready()) return "";
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT data_json FROM podcast_cache WHERE feed_url=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";

    sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);
    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        result = data ? data : "";
    }
    sqlite3_finalize(stmt);
    return result;
}

void DatabaseManager::prepared_delete_podcast_cache(const std::string& feed_url)
{
    const char* sql = "DELETE FROM podcast_cache WHERE feed_url=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::delete_podcast_cache(const std::string& feed_url)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_delete_podcast_cache(feed_url);
    LOG(fmt::format("[DB] Deleted podcast cache: {}", feed_url));
}

// ====================================================================
// Episode cache
// ====================================================================

void DatabaseManager::save_episode_cache(const std::string& feed_url,
                                          const std::string& episodes_json)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);

    // Delete old cache for this feed (prepared)
    {
        const char* del_sql = "DELETE FROM episode_cache WHERE feed_url=?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, del_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // Parse JSON and save each episode individually (prepared)
    try {
        json j = json::parse(episodes_json);
        if (j.is_array()) {
            const char* ins_sql =
                "INSERT INTO episode_cache "
                "(feed_url, episode_url, title, duration, pub_date, data_json) "
                "VALUES (?, ?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db_, ins_sql, -1, &stmt, nullptr) == SQLITE_OK) {
                for (const auto& ep : j) {
                    std::string ep_url     = ep.value("url", "");
                    std::string ep_title   = ep.value("title", "");
                    int ep_duration        = ep.value("duration", 0);
                    std::string ep_pub_date = ep.value("pubDate", "");

                    sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 2, ep_url.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 3, ep_title.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(stmt, 4, ep_duration);
                    sqlite3_bind_text(stmt, 5, ep_pub_date.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 6, ep.dump().c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(stmt);
                    sqlite3_reset(stmt);
                }
                sqlite3_finalize(stmt);
            }
        }
    } catch (...) {}
}

bool DatabaseManager::is_episode_cached(const std::string& feed_url)
{
    if (!is_ready()) return false;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT COUNT(*) FROM episode_cache WHERE feed_url=?;";
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count > 0;
}

std::string DatabaseManager::load_episode_cache(const std::string& feed_url)
{
    if (!is_ready()) return "";
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql =
        "SELECT data_json FROM episode_cache WHERE feed_url=? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";

    sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);

    json episodes = json::array();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (data) {
            try {
                episodes.push_back(json::parse(data));
            } catch (...) {}
        }
    }
    sqlite3_finalize(stmt);
    return episodes.dump();
}

std::vector<std::tuple<std::string, std::string, int, std::string>>
DatabaseManager::load_episodes_from_cache(const std::string& feed_url)
{
    std::vector<std::tuple<std::string, std::string, int, std::string>> episodes;
    if (!is_ready()) return episodes;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql =
        "SELECT episode_url, title, duration, data_json "
        "FROM episode_cache WHERE feed_url=? ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return episodes;

    sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* ep_url   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* ep_title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        int ep_duration      = sqlite3_column_int(stmt, 2);
        const char* ep_data  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        episodes.push_back({
            ep_url   ? ep_url   : "",
            ep_title ? ep_title : "",
            ep_duration,
            ep_data  ? ep_data  : ""
        });
    }
    sqlite3_finalize(stmt);
    return episodes;
}

void DatabaseManager::prepared_delete_episode_cache_by_feed(const std::string& feed_url)
{
    const char* sql = "DELETE FROM episode_cache WHERE feed_url=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, feed_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::delete_episode_cache_by_feed(const std::string& feed_url)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_delete_episode_cache_by_feed(feed_url);
    LOG(fmt::format("[DB] Deleted episode cache for feed: {}", feed_url));
}

// ====================================================================
// Player state
// ====================================================================

void DatabaseManager::prepared_save_player_state(int volume, double speed, bool paused,
                                                  const std::string& url, double position,
                                                  bool scroll_mode, bool show_tree_lines,
                                                  const std::string& current_title,
                                                  int current_mode)
{
    const char* sql =
        "INSERT OR REPLACE INTO player_state "
        "(id, volume, speed, paused, current_url, position, scroll_mode, "
        "show_tree_lines, current_title, current_mode) "
        "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, volume);
    sqlite3_bind_double(stmt, 2, speed);
    sqlite3_bind_int(stmt, 3, paused ? 1 : 0);
    sqlite3_bind_text(stmt, 4, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, position);
    sqlite3_bind_int(stmt, 6, scroll_mode ? 1 : 0);
    sqlite3_bind_int(stmt, 7, show_tree_lines ? 1 : 0);
    sqlite3_bind_text(stmt, 8, current_title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, current_mode);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::save_player_state(int volume, double speed, bool paused,
                                         const std::string& url, double position,
                                         bool scroll_mode, bool show_tree_lines,
                                         const std::string& current_title, int current_mode)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_save_player_state(volume, speed, paused, url, position,
                               scroll_mode, show_tree_lines, current_title, current_mode);
}

PlayerStateData DatabaseManager::load_player_state()
{
    PlayerStateData state;
    if (!is_ready()) return state;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql =
        "SELECT volume, speed, paused, current_url, position, "
        "scroll_mode, show_tree_lines, current_title, current_mode "
        "FROM player_state WHERE id=1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return state;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        state.volume        = sqlite3_column_int(stmt, 0);
        state.speed         = sqlite3_column_double(stmt, 1);
        state.paused        = sqlite3_column_int(stmt, 2) != 0;
        const char* url     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        state.current_url   = url ? url : "";
        state.position      = sqlite3_column_double(stmt, 4);
        state.scroll_mode   = sqlite3_column_int(stmt, 5) != 0;
        state.show_tree_lines = sqlite3_column_int(stmt, 6) != 0;
        const char* title   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        state.current_title = title ? title : "";
        state.current_mode  = sqlite3_column_int(stmt, 8);
    }
    sqlite3_finalize(stmt);
    return state;
}

// ====================================================================
// Playlist persistence
// ====================================================================

void DatabaseManager::prepared_save_playlist_item(const SavedPlaylistItem& item)
{
    const char* sql =
        "INSERT INTO playlist (title, url, node_path, duration, is_video, sort_order) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, item.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, item.url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, item.node_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, item.duration);
    sqlite3_bind_int(stmt, 5, item.is_video ? 1 : 0);
    sqlite3_bind_int(stmt, 6, item.sort_order);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::save_playlist_item(const SavedPlaylistItem& item)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_save_playlist_item(item);
}

std::vector<SavedPlaylistItem> DatabaseManager::load_playlist()
{
    std::vector<SavedPlaylistItem> items;
    if (!is_ready()) return items;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql =
        "SELECT id, title, url, node_path, duration, is_video, sort_order "
        "FROM playlist ORDER BY sort_order;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return items;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SavedPlaylistItem item;
        item.id = sqlite3_column_int(stmt, 0);
        const char* title     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        item.title = title ? title : "";
        const char* url       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        item.url = url ? url : "";
        const char* node_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        item.node_path = node_path ? node_path : "";
        item.duration = sqlite3_column_int(stmt, 4);
        item.is_video = sqlite3_column_int(stmt, 5) != 0;
        item.sort_order = sqlite3_column_int(stmt, 6);
        items.push_back(item);
    }
    sqlite3_finalize(stmt);
    return items;
}

void DatabaseManager::delete_playlist_item(int id)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "DELETE FROM playlist WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::clear_playlist_table()
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    exec_sql("DELETE FROM playlist;");
    LOG("[DB] Playlist cleared");
}

void DatabaseManager::update_playlist_order(int id, int new_order)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "UPDATE playlist SET sort_order=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, new_order);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::reorder_playlist()
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);

    // Fetch all IDs in current order and reassign sequential sort_order
    const char* sel_sql = "SELECT id FROM playlist ORDER BY sort_order;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sel_sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    const char* upd_sql = "UPDATE playlist SET sort_order=? WHERE id=?;";
    sqlite3_stmt* upd_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, upd_sql, -1, &upd_stmt, nullptr) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return;
    }

    int order = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        sqlite3_bind_int(upd_stmt, 1, order++);
        sqlite3_bind_int(upd_stmt, 2, id);
        sqlite3_step(upd_stmt);
        sqlite3_reset(upd_stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_finalize(upd_stmt);
}

// ====================================================================
// Radio tree cache
// ====================================================================

void DatabaseManager::save_radio_cache(const TreeNodePtr& root)
{
    if (!is_ready() || !root) return;
    std::lock_guard<std::mutex> lock(mtx_);

    // Clear old data
    exec_sql("DELETE FROM radio_cache;");

    // Recursively save nodes
    int order = 0;
    save_radio_node_recursive(root, 0, order);
}

void DatabaseManager::save_radio_node_recursive(const TreeNodePtr& node,
                                                 int parent_id, int& order)
{
    if (!node) return;

    for (const auto& child : node->children) {
        // Insert current node using prepared statement
        const char* sql =
            "INSERT INTO radio_cache "
            "(parent_id, title, url, type, expanded, children_loaded, "
            "is_youtube, channel_name, is_cached, is_downloaded, local_file, sort_order) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, parent_id);
            sqlite3_bind_text(stmt, 2, child->title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, child->url.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 4, static_cast<int>(child->type));
            sqlite3_bind_int(stmt, 5, child->expanded ? 1 : 0);
            sqlite3_bind_int(stmt, 6, child->children_loaded ? 1 : 0);
            sqlite3_bind_int(stmt, 7, child->is_youtube ? 1 : 0);
            sqlite3_bind_text(stmt, 8, child->channel_name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 9, child->is_cached ? 1 : 0);
            sqlite3_bind_int(stmt, 10, child->is_downloaded ? 1 : 0);
            sqlite3_bind_text(stmt, 11, child->local_file.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 12, order++);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Recurse into children
        int64_t new_id = sqlite3_last_insert_rowid(db_);
        save_radio_node_recursive(child, static_cast<int>(new_id), order);
    }
}

TreeNodePtr DatabaseManager::load_radio_cache()
{
    if (!is_ready()) return nullptr;
    std::lock_guard<std::mutex> lock(mtx_);

    // Create virtual root
    auto root = std::make_shared<TreeNode>();
    root->title = "Radio";
    root->type = NodeType::FOLDER;
    root->expanded = true;

    // Load top-level nodes (parent_id=0)
    load_radio_node_recursive(root, 0);

    return root;
}

void DatabaseManager::load_radio_node_recursive(const TreeNodePtr& parent, int parent_id)
{
    const char* sql =
        "SELECT id, title, url, type, expanded, children_loaded, is_youtube, channel_name, "
        "is_cached, is_downloaded, local_file FROM radio_cache WHERE parent_id=? "
        "ORDER BY sort_order;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int(stmt, 1, parent_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto node = std::make_shared<TreeNode>();
        int node_id = sqlite3_column_int(stmt, 0);

        const char* title        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* url          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int type                 = sqlite3_column_int(stmt, 3);
        bool expanded            = sqlite3_column_int(stmt, 4) != 0;
        bool children_loaded     = sqlite3_column_int(stmt, 5) != 0;
        bool is_youtube          = sqlite3_column_int(stmt, 6) != 0;
        const char* channel_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        bool is_cached           = sqlite3_column_int(stmt, 8) != 0;
        bool is_downloaded       = sqlite3_column_int(stmt, 9) != 0;
        const char* local_file   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));

        node->title          = title ? title : "";
        node->url            = url ? url : "";
        node->type           = static_cast<NodeType>(type);
        node->expanded       = expanded;
        node->children_loaded = children_loaded;
        node->is_youtube     = is_youtube;
        node->channel_name   = channel_name ? channel_name : "";
        node->is_cached      = is_cached;
        node->is_downloaded  = is_downloaded;
        node->local_file     = local_file ? local_file : "";

        // Set parent pointer
        node->parent = parent;
        parent->children.push_back(node);

        // Recursively load children
        load_radio_node_recursive(node, node_id);
    }
    sqlite3_finalize(stmt);
}

// ====================================================================
// History cleanup (uses INI config values)
// ====================================================================

void DatabaseManager::cleanup_old_history()
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);

    // Read configuration from INI (default values match the originals)
    int max_days = 1080;
    int max_records = 2048;

    // Purge by age (if configured > 0)
    if (max_days > 0) {
        std::string sql_days = fmt::format(
            "DELETE FROM history WHERE timestamp < datetime('now', '-{} days');",
            max_days
        );
        exec_sql(sql_days);
    }

    // Purge by count (if configured > 0)
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

// ====================================================================
// URL cache (replaces former cached_urls.json)
// All mutations use prepared statements to prevent SQL injection.
// ====================================================================

void DatabaseManager::prepared_url_cache_upsert(const std::string& url,
                                                int is_cached,
                                                int is_downloaded,
                                                const std::string& local_file)
{
    const char* sql =
        "INSERT INTO url_cache (url, is_cached, is_downloaded, local_file) "
        "VALUES (?, ?, ?, ?) "
        "ON CONFLICT(url) DO UPDATE SET "
        "is_cached = MAX(is_cached, excluded.is_cached), "
        "is_downloaded = MAX(is_downloaded, excluded.is_downloaded), "
        "local_file = CASE WHEN excluded.local_file != '' THEN excluded.local_file "
        "ELSE url_cache.local_file END, "
        "updated_at = CURRENT_TIMESTAMP;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, is_cached);
    sqlite3_bind_int(stmt, 3, is_downloaded);
    sqlite3_bind_text(stmt, 4, local_file.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::prepared_url_cache_clear(const std::string& url)
{
    const char* sql = "DELETE FROM url_cache WHERE url = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DatabaseManager::url_cache_mark_cached(const std::string& url)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_url_cache_upsert(url, /*is_cached=*/1, /*is_downloaded=*/0, "");
}

void DatabaseManager::url_cache_mark_downloaded(const std::string& url,
                                                 const std::string& local_file)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_url_cache_upsert(url, /*is_cached=*/0, /*is_downloaded=*/1, local_file);
}

bool DatabaseManager::url_cache_is_cached(const std::string& url)
{
    if (!is_ready()) return false;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT is_cached FROM url_cache WHERE url = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0) != 0;
    }
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::url_cache_is_downloaded(const std::string& url)
{
    if (!is_ready()) return false;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT is_downloaded FROM url_cache WHERE url = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0) != 0;
    }
    sqlite3_finalize(stmt);
    return result;
}

std::string DatabaseManager::url_cache_get_local_file(const std::string& url)
{
    if (!is_ready()) return "";
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT local_file FROM url_cache WHERE url = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
    sqlite3_bind_text(stmt, 1, url.c_str(), -1, SQLITE_TRANSIENT);

    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        result = val ? val : "";
    }
    sqlite3_finalize(stmt);
    return result;
}

void DatabaseManager::url_cache_clear_cached(const std::string& url)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_url_cache_clear(url);
}

void DatabaseManager::url_cache_clear_downloaded(const std::string& url)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);
    prepared_url_cache_clear(url);
}

void DatabaseManager::url_cache_load_all(std::set<std::string>& cached,
                                           std::set<std::string>& downloaded,
                                           std::map<std::string, std::string>& local_files)
{
    if (!is_ready()) return;
    std::lock_guard<std::mutex> lock(mtx_);

    const char* sql = "SELECT url, is_cached, is_downloaded, local_file FROM url_cache;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (!url) continue;
        std::string s_url = url;
        int is_c  = sqlite3_column_int(stmt, 1);
        int is_d  = sqlite3_column_int(stmt, 2);
        const char* lf = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        if (is_c) cached.insert(s_url);
        if (is_d) downloaded.insert(s_url);
        if (lf && lf[0] != '\0') local_files[s_url] = lf;
    }
    sqlite3_finalize(stmt);
}

} // namespace podradio
