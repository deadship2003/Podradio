#pragma once

#include "common.h"
#include "mpv_controller.h"
#include "ui.h"

namespace podradio
{

// =========================================================
// App - Main application controller
// Manages the event loop, tree data, playback, and all UI input.
// =========================================================
class App {
public:
    App();

    // Main TUI event loop
    void run();

    // CLI command-line operations
    void import_feed(const std::string& url);
    void import_opml(const std::string& filepath);
    void export_podcasts(const std::string& filepath);
    void load_data();

private:
    // =========================================================
    // Core objects
    // =========================================================
    UI ui;
    MPVController player;

    // Tree root nodes
    TreeNodePtr radio_root;
    TreeNodePtr podcast_root;
    TreeNodePtr fav_root;
    TreeNodePtr history_root;
    TreeNodePtr current_root;
    TreeNodePtr playback_node;

    // Display state
    std::vector<DisplayItem> display_list;
    int selected_idx = 0;
    int view_start = 0;
    bool running = true;
    AppMode mode = AppMode::RADIO;
    std::recursive_mutex tree_mutex;

    // Search state
    std::string search_query;
    std::vector<TreeNodePtr> search_matches;
    int current_match_idx = -1;
    int total_matches = 0;

    // Visual mode (v key multi-select)
    bool visual_mode_ = false;
    int visual_start_ = -1;

    // =========================================================
    // Playlist management
    // =========================================================
    std::vector<PlaylistItem> current_playlist;
    int current_playlist_index = -1;

    // V0.05B9n3e5g3h: Shuffle map for RANDOM play mode
    // shuffle_map_[mpv_pos] = original_index
    std::vector<size_t> shuffle_map_;

    // =========================================================
    // List mode (playlist management popup)
    // =========================================================
    std::vector<SavedPlaylistItem> saved_playlist;
    int list_selected_idx = 0;
    bool in_list_mode_ = false;
    std::set<int> list_marked_indices;
    bool list_visual_mode_ = false;
    int list_visual_start_ = -1;
    std::string list_search_filter_;
    PlayMode play_mode = PlayMode::SINGLE;

    // Cursor sync tracking
    int last_mpv_playlist_pos_ = -1;

    // Sort state for L-list
    bool saved_playlist_sort_reversed_ = false;

    // =========================================================
    // Inner struct for processed playlist
    // =========================================================
    struct ProcessedPlaylist {
        std::vector<std::string> urls;
        int start_idx;
        bool loop_single;
    };

    // =========================================================
    // Cursor synchronization
    // =========================================================
    void sync_list_cursor_from_mpv(const MPVController::State& state);

    // =========================================================
    // Auto-play next track
    // =========================================================
    bool is_playable_node(TreeNodePtr node);
    TreeNodePtr find_next_playable_sibling(TreeNodePtr current);
    void on_playback_ended(int reason);

    // =========================================================
    // Playlist operations
    // =========================================================
    void clear_playlist();
    void load_saved_playlist();
    void save_current_playlist_to_db();
    void add_to_saved_playlist(TreeNodePtr node);
    void delete_from_saved_playlist(int idx);
    void clear_saved_playlist();
    void move_playlist_item_up(int idx);
    void move_playlist_item_down(int idx);

    // =========================================================
    // List mode playback (modular architecture)
    // =========================================================
    void play_saved_playlist_item(int idx);
    ProcessedPlaylist apply_play_mode_for_list(const std::vector<std::string>& urls, int current_idx, PlayMode mode);
    void execute_play_for_list(const std::vector<std::string>& urls, int start_idx, bool has_video, bool loop_single);

    // =========================================================
    // Cache marking
    // =========================================================
    void mark_cached_nodes(TreeNodePtr node);

    // =========================================================
    // Input handling
    // =========================================================
    void handle_input(int ch, int marked_count);

    // =========================================================
    // Navigation
    // =========================================================
    void nav_up();
    void nav_down();
    void nav_top();
    void nav_bottom();
    void nav_page_up();
    void nav_page_down();
    void go_back();

    // =========================================================
    // Node operations
    // =========================================================
    void enter_node(int marked_count);
    void toggle_mark();
    void clear_all_marks();
    void confirm_visual_selection();
    void delete_node(int marked_count);
    void download_node(int marked_count);
    void add_favourite();
    void add_favourites_batch(int marked_count);
    void edit_node();
    void refresh_node();
    void add_feed();

    // =========================================================
    // Playlist construction
    // =========================================================
    int find_url_in_saved_playlist(const std::string& url);
    std::vector<std::string> build_playlist_from_siblings_and_save(int& current_idx, bool& has_video, TreeNodePtr current);
    std::vector<std::string> build_playlist_from_saved(int& current_idx, bool& has_video, TreeNodePtr current_node);
    std::vector<std::string> build_playlist_from_siblings(int& current_idx, bool& has_video, TreeNodePtr current);

    // =========================================================
    // Play mode processing
    // =========================================================
    ProcessedPlaylist apply_play_mode(const std::vector<std::string>& urls, int current_idx, PlayMode mode);

    // =========================================================
    // Play execution
    // =========================================================
    void execute_play(const std::vector<std::string>& urls, int start_idx, bool has_video, bool loop_single);

    // =========================================================
    // Sorting
    // =========================================================
    void toggle_saved_playlist_sort();
    void toggle_sort_order();
    static bool title_compare_asc(const std::string& a, const std::string& b);
    static bool title_compare_desc(const std::string& a, const std::string& b);

    // =========================================================
    // Mark / collect helpers
    // =========================================================
    int count_marked_safe(const TreeNodePtr& node);
    int count_marked(const TreeNodePtr& node);
    void collect_playable_marked(const TreeNodePtr& node, std::vector<TreeNodePtr>& list);
    void collect_playable_items(const TreeNodePtr& node, std::vector<TreeNodePtr>& list);
    void clear_marks(const TreeNodePtr& node);
    void collect_marked(const TreeNodePtr& node, std::vector<TreeNodePtr>& list);
    bool remove_node(TreeNodePtr parent, TreeNodePtr target);

    // =========================================================
    // Search
    // =========================================================
    void reset_search();
    void perform_search();
    void search_recursive(const TreeNodePtr& node, const std::string& query, std::vector<TreeNodePtr>& results);
    void jump_search(int dir);
    void jump_to_match(int idx);
    void reveal_node(TreeNodePtr node);

    // =========================================================
    // Online search
    // =========================================================
    void perform_online_search();
    void perform_online_search_from_favourite();
    void load_search_history_children(TreeNodePtr node);

    // =========================================================
    // Favourite / cache loading
    // =========================================================
    bool load_favourite_children_from_cache(TreeNodePtr node);

    // =========================================================
    // Subscribe
    // =========================================================
    void subscribe_online_podcast();
    void subscribe_online_podcasts_batch(int marked_count);
    void subscribe_favourite_single();
    void subscribe_favourites_batch(int marked_count);

    // =========================================================
    // LINK mechanism (favourites as soft links)
    // =========================================================
    TreeNodePtr get_root_by_mode_string(const std::string& mode_str);
    bool should_use_radio_loader(const std::string& source_mode, NodeType node_type, const std::string& url);
    void sync_link_node_status(TreeNodePtr target);
    bool expand_link_node(TreeNodePtr node);
    TreeNodePtr find_node_by_url(TreeNodePtr root, const std::string& url);

    // =========================================================
    // History
    // =========================================================
    void load_history_to_root();
    void record_play_history(const std::string& url, const std::string& title, int duration = 0);

    // =========================================================
    // Persistence
    // =========================================================
    void load_persistent_data();
    void save_persistent_data();
    void restore_player_state();

    // =========================================================
    // Data loading
    // =========================================================
    void load_radio_root();
    void spawn_load_radio(TreeNodePtr node, bool force = false);
    void spawn_load_feed(TreeNodePtr node);
    void load_default_podcasts();

    // =========================================================
    // Tree flattening
    // =========================================================
    void flatten(const TreeNodePtr& node, int depth, bool is_last, int parent_idx);

    // =========================================================
    // Cache clearing
    // =========================================================
    void clear_feed_cache(TreeNodePtr feed);
};

} // namespace podradio
