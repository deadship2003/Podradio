#pragma once

/*
 * database.h - Database Manager (SQLite persistence layer)
 *
 * Singleton class managing all persistent storage via SQLite.
 * Thread-safe: all public methods acquire an internal mutex.
 *
 * Security: User-supplied data is bound via prepared statements
 * (prepared_* methods). The legacy escape_sql() path is deprecated.
 */

#include "common.h"
#include <mutex>
#include <set>
#include <map>
#include <utility>
#include <vector>
#include <tuple>
#include <string>

namespace podradio
{

// Search history item (for Online mode display)
struct SearchHistoryItem {
    int id;
    std::string query;
    std::string region;
    std::string timestamp;
    int result_count;
};

// Player state snapshot (saved/restored across sessions)
struct PlayerStateData {
    int volume = 100;
    double speed = 1.0;
    bool paused = false;
    std::string current_url;
    double position = 0;
    bool scroll_mode = false;
    bool show_tree_lines = true;
    std::string current_title;
    int current_mode = 0;
};

// =========================================================
// DatabaseManager - SQLite persistence singleton
// =========================================================
class DatabaseManager
{
public:
    static DatabaseManager& instance() { static DatabaseManager db; return db; }

    // --- Lifecycle ----------------------------------------------------------
    bool init();
    ~DatabaseManager();
    bool is_ready() const;

    // --- Progress -----------------------------------------------------------
    void save_progress(const std::string& url, double position, bool completed);
    std::pair<double, bool> get_progress(const std::string& url);

    // --- History ------------------------------------------------------------
    void add_history(const std::string& url, const std::string& title, int duration);
    std::vector<std::tuple<std::string, std::string, std::string>> get_history(int limit = 50);
    void delete_history(const std::string& url);

    // --- Statistics ---------------------------------------------------------
    void increment_stat(const std::string& key, int64_t delta);
    int64_t get_stat(const std::string& key, int64_t default_val = 0);

    // --- Generic URL cache --------------------------------------------------
    void cache_url_data(const std::string& url, const std::string& json_data);
    std::string get_cached_data(const std::string& url);

    // --- Nodes (subscriptions) ----------------------------------------------
    void save_node(const std::string& url, const std::string& title, int type, const std::string& json_data);
    std::vector<std::tuple<std::string, std::string, int, std::string>> load_nodes();
    void clear_nodes();

    // --- Purge all tables ---------------------------------------------------
    void purge_all();

    // --- Search cache -------------------------------------------------------
    void save_search_cache(const std::string& query, const std::string& region, const std::string& result_json);
    std::vector<SearchHistoryItem> load_all_search_history();
    std::string load_search_cache(const std::string& query, const std::string& region);
    void delete_search_history(const std::string& query, const std::string& region);

    // --- Favourites ---------------------------------------------------------
    void save_favourite(const std::string& title, const std::string& url, int type,
                        bool is_youtube = false, const std::string& channel_name = "",
                        const std::string& source_type = "", const std::string& data_json = "");
    std::vector<std::tuple<std::string, std::string, int, bool, std::string, std::string, std::string>> load_favourites();
    void delete_favourite(const std::string& url);
    bool is_favourite(const std::string& url);
    void clear_favourites();

    // --- Podcast feed cache -------------------------------------------------
    void save_podcast_cache(const std::string& feed_url, const std::string& title,
                            const std::string& artist, const std::string& genre,
                            int track_count, const std::string& artwork_url,
                            int collection_id, const std::string& data_json);
    bool is_podcast_cached(const std::string& feed_url);
    std::string load_podcast_cache(const std::string& feed_url);
    void delete_podcast_cache(const std::string& feed_url);

    // --- Episode cache ------------------------------------------------------
    void save_episode_cache(const std::string& feed_url, const std::string& episodes_json);
    bool is_episode_cached(const std::string& feed_url);
    std::string load_episode_cache(const std::string& feed_url);
    std::vector<std::tuple<std::string, std::string, int, std::string>> load_episodes_from_cache(const std::string& feed_url);
    void delete_episode_cache_by_feed(const std::string& feed_url);

    // --- Player state -------------------------------------------------------
    void save_player_state(int volume, double speed, bool paused, const std::string& url = "",
                           double position = 0, bool scroll_mode = false, bool show_tree_lines = true,
                           const std::string& current_title = "", int current_mode = 0);
    PlayerStateData load_player_state();

    // --- Playlist -----------------------------------------------------------
    void save_playlist_item(const SavedPlaylistItem& item);
    std::vector<SavedPlaylistItem> load_playlist();
    void delete_playlist_item(int id);
    void clear_playlist_table();
    void update_playlist_order(int id, int new_order);
    void reorder_playlist();

    // --- Radio tree cache ---------------------------------------------------
    void save_radio_cache(const TreeNodePtr& root);
    TreeNodePtr load_radio_cache();

    // --- URL cache (replaces former cached_urls.json) ----------------------
    void url_cache_mark_cached(const std::string& url);
    void url_cache_mark_downloaded(const std::string& url, const std::string& local_file = "");
    bool url_cache_is_cached(const std::string& url);
    bool url_cache_is_downloaded(const std::string& url);
    std::string url_cache_get_local_file(const std::string& url);
    void url_cache_clear_cached(const std::string& url);
    void url_cache_clear_downloaded(const std::string& url);
    void url_cache_load_all(std::set<std::string>& cached,
                             std::set<std::string>& downloaded,
                             std::map<std::string, std::string>& local_files);

    // --- History cleanup ----------------------------------------------------
    void cleanup_old_history();

private:
    DatabaseManager() : initialized_(false), db_(nullptr) {}
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool initialized_ = false;
    sqlite3* db_ = nullptr;
    std::mutex mtx_;

    // --- Low-level helpers --------------------------------------------------
    [[deprecated("Use prepared statements for user-supplied data")]]
    std::string escape_sql(const std::string& s);

    bool exec_sql(const std::string& sql);
    bool exec_sql_raw(const std::string& sql);  // no logging wrapper (for static SQL)

    // --- Prepared-statement helpers (security: SQL injection prevention) -----
    void prepared_save_progress(const std::string& url, double pos, bool completed);
    void prepared_add_history(const std::string& url, const std::string& title, int duration);
    void prepared_save_favourite(const std::string& title, const std::string& url, int type,
                                 bool is_youtube, const std::string& channel_name,
                                 const std::string& source_type, const std::string& data_json);
    void prepared_delete_favourite(const std::string& url);
    void prepared_delete_history(const std::string& url);
    void prepared_delete_search_history(const std::string& query, const std::string& region);
    void prepared_save_search_cache(const std::string& query, const std::string& region,
                                    const std::string& result_json, bool exists);
    void prepared_save_podcast_cache(const std::string& feed_url, const std::string& title,
                                     const std::string& artist, const std::string& genre,
                                     int track_count, const std::string& artwork_url,
                                     int collection_id, const std::string& data_json);
    void prepared_save_player_state(int volume, double speed, bool paused,
                                    const std::string& url, double position,
                                    bool scroll_mode, bool show_tree_lines,
                                    const std::string& current_title, int current_mode);
    void prepared_save_playlist_item(const SavedPlaylistItem& item);
    void prepared_save_node(const std::string& url, const std::string& title,
                            int type, const std::string& json_data);
    void prepared_cache_url_data(const std::string& url, const std::string& json_data);
    void prepared_increment_stat(const std::string& key, int64_t delta);
    void prepared_delete_podcast_cache(const std::string& feed_url);
    void prepared_delete_episode_cache_by_feed(const std::string& feed_url);
    void prepared_url_cache_upsert(const std::string& url, int is_cached,
                                   int is_downloaded, const std::string& local_file);
    void prepared_url_cache_clear(const std::string& url);

    // --- Recursive radio tree helpers ---------------------------------------
    void save_radio_node_recursive(const TreeNodePtr& node, int parent_id, int& order);
    void load_radio_node_recursive(const TreeNodePtr& parent, int parent_id);
};

} // namespace podradio
