/*
 * app.cpp - Main application controller implementation
 *
 * Contains the full App class implementation including:
 * - Constructor / run loop
 * - Tree navigation and node operations
 * - Playlist management (L-mode)
 * - Playback mode processing (SINGLE/CYCLE/RANDOM)
 * - Online search, favourites, LINK mechanism
 * - Import/export, download management
 * - Visual mode, search, sorting
 */

#include "podradio/app.h"
#include "podradio/logger.h"
#include "podradio/config.h"
#include "podradio/database.h"
#include "podradio/cache_manager.h"
#include "podradio/youtube_cache.h"
#include "podradio/network.h"
#include "podradio/parsers.h"
#include "podradio/persistence.h"
#include "podradio/url_classifier.h"
#include "podradio/utils.h"
#include "podradio/progress.h"
#include "podradio/event_log.h"
#include "podradio/itunes_search.h"
#include "podradio/online_state.h"
#include "podradio/icon_manager.h"
#include "podradio/layout.h"
#include "podradio/sleep_timer.h"

// LOG and EVENT_LOG macros are defined in logger.h / event_log.h

namespace podradio
{

// =========================================================
// Constructor
// =========================================================
App::App() {
    Logger::instance().init();
    IniConfig::instance().load();
    // V0.03: Initialize SQLite3 database
    DatabaseManager::instance().init();
    CacheManager::instance().load();  // Load url_cache table into memory
    YouTubeCache::instance().load();

    // Initialize root nodes in constructor to avoid uninitialized state in CLI mode
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

// =========================================================
// run() - Main TUI event loop
// =========================================================
void App::run() {
    ui.init();
    // Confirm XML error handler is set (also set in main)
    xmlSetGenericErrorFunc(NULL, xml_error_handler);
    xmlSetStructuredErrorFunc(NULL, xml_structured_error_handler);
    if (!player.initialize()) LOG("MPV initialization failed");

    // Register end-of-file callback for auto-play next track
    player.set_end_file_callback([this](int reason) {
        on_playback_ended(reason);
    });
    LOG("[AUTOPLAY] End-file callback registered");

    current_root = radio_root;

    Persistence::load_cache(radio_root, podcast_root);
    load_persistent_data();

    // Load history records into history_root
    load_history_to_root();

    // Restore previous playback state
    restore_player_state();

    mark_cached_nodes(radio_root);
    mark_cached_nodes(podcast_root);

    if (radio_root->children.empty()) {
        std::thread([this]() { load_radio_root(); }).detach();
    }
    // V2.39-FF: Always load built-in podcasts, merge with cached data
    load_default_podcasts();

    // Initialize layout manager
    LayoutMetrics::instance().check_resize();

    while (running) {
        // Check SIGINT (CTRL+C) exit request
        if (g_exit_requested) {
            g_exit_requested = false;
            if (ui.confirm_box("Quit PODRADIO? (CTRL+C)")) {
                running = false;
                break;
            }
        }

        // Check sleep timer
        if (SleepTimer::instance().is_active() && SleepTimer::instance().check_expired()) {
            EVENT_LOG("Sleep timer expired, exiting...");
            running = false;
            break;
        }

        // Unified window size detection and scroll offset reset
        if (LayoutMetrics::instance().check_resize()) {
            resizeterm(LINES, COLS);
            clear();
            refresh();
            EVENT_LOG(fmt::format("Terminal resized: {}x{}", COLS, LINES));
        }

        auto state = player.get_state();

        // V0.05B9n3e5g3k: Fix cursor sync in SINGLE mode
        if (state.has_media && !current_playlist.empty() &&
            state.playlist_pos >= 0 && state.playlist_pos < (int)current_playlist.size()) {

            int new_index = -1;

            if (play_mode == PlayMode::RANDOM && !shuffle_map_.empty() &&
                state.playlist_pos < (int)shuffle_map_.size()) {
                new_index = static_cast<int>(shuffle_map_[state.playlist_pos]);
            } else if (play_mode == PlayMode::CYCLE) {
                new_index = state.playlist_pos;
            }

            if (new_index >= 0 && current_playlist_index != new_index) {
                current_playlist_index = new_index;
                if (!saved_playlist.empty() && new_index >= 0 && new_index < (int)saved_playlist.size()) {
                    list_selected_idx = new_index;
                }
                LOG(fmt::format("[SYNC] Playlist index synced to {} (mpv_pos={}, mode={})",
                    current_playlist_index, state.playlist_pos,
                    play_mode == PlayMode::RANDOM ? "RANDOM" : "CYCLE"));
            }
        }

        // Determine application state
        AppState app_state;
        if (in_list_mode_) {
            app_state = AppState::LIST_MODE;
        } else if (state.has_media) {
            if (state.core_idle && !state.paused) {
                app_state = AppState::BUFFERING;
            } else if (state.paused) {
                app_state = AppState::PAUSED;
            } else {
                app_state = AppState::PLAYING;
            }
        } else {
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

        sync_list_cursor_from_mpv(state);

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
    // CacheManager auto-persists to DB on each mark_* call; no explicit save() needed.

    // Save player state
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

// =========================================================
// CLI operations (using std::cout for CLI mode compatibility)
// =========================================================

void App::import_feed(const std::string& url) {
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

void App::import_opml(const std::string& filepath) {
    std::cout << "Importing from: " << filepath << std::endl;

    auto feeds = OPMLParser::import_opml_file(filepath);
    if (feeds.empty()) {
        std::cout << "No feeds found in OPML file" << std::endl;
        return;
    }

    std::cout << "Found " << feeds.size() << " feeds in OPML" << std::endl;

    int count = 0;
    for (auto& feed : feeds) {
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

void App::export_podcasts(const std::string& filename) {
    Persistence::export_opml(filename, podcast_root->children);
}

void App::load_data() {
    Persistence::load_cache(radio_root, podcast_root);
    load_persistent_data();
    std::cout << "Loaded " << podcast_root->children.size() << " podcasts from cache" << std::endl;
}

// =========================================================
// Cursor synchronization from MPV state
// =========================================================
void App::sync_list_cursor_from_mpv(const MPVController::State& state) {
    if (saved_playlist.empty() || !state.has_media) return;

    int mpv_pos = state.playlist_pos;

    if (mpv_pos == last_mpv_playlist_pos_) return;
    last_mpv_playlist_pos_ = mpv_pos;

    int target_idx = -1;

    switch (play_mode) {
        case PlayMode::SINGLE:
            target_idx = current_playlist_index;
            break;

        case PlayMode::CYCLE:
            if (mpv_pos >= 0 && mpv_pos < static_cast<int>(current_playlist.size())) {
                target_idx = mpv_pos;
                current_playlist_index = target_idx;
            }
            break;

        case PlayMode::RANDOM:
            if (mpv_pos >= 0 && mpv_pos < static_cast<int>(shuffle_map_.size())) {
                target_idx = static_cast<int>(shuffle_map_[mpv_pos]);
                current_playlist_index = target_idx;
            }
            break;
    }

    if (target_idx >= 0 && target_idx < static_cast<int>(saved_playlist.size())) {
        if (list_selected_idx != target_idx) {
            list_selected_idx = target_idx;
            LOG(fmt::format("[SYNC] Cursor synced to idx {}", target_idx));
        }
    }
}

// =========================================================
// Auto-play next track
// =========================================================
bool App::is_playable_node(TreeNodePtr node) {
    if (!node) return false;
    return !node->url.empty();
}

TreeNodePtr App::find_next_playable_sibling(TreeNodePtr current) {
    if (!current) return nullptr;

    auto parent = current->parent.lock();
    if (!parent) return nullptr;

    auto& siblings = parent->children;

    size_t current_idx = 0;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == current.get()) {
            current_idx = i;
            break;
        }
    }

    int direction = parent->sort_reversed ? -1 : 1;
    int next_idx = static_cast<int>(current_idx) + direction;

    while (next_idx >= 0 && next_idx < static_cast<int>(siblings.size())) {
        if (is_playable_node(siblings[next_idx])) {
            return siblings[next_idx];
        }
        next_idx += direction;
    }

    return nullptr;
}

void App::on_playback_ended(int reason) {
    if (reason != 0) {
        LOG(fmt::format("[AUTOPLAY] End file reason={}, ignored", reason));
        return;
    }

    auto state = player.get_state();
    int pl_pos = state.playlist_pos;
    int pl_count = state.playlist_count;

    LOG(fmt::format("[AUTOPLAY] Track ended, playlist_pos={}, count={}", pl_pos, pl_count));

    bool is_last_item = (pl_count <= 1) || (pl_pos < 0) || (pl_pos >= pl_count - 1);

    if (!is_last_item) {
        LOG("[AUTOPLAY] MPV will auto-play next item");
        return;
    }

    LOG("[AUTOPLAY] Playlist ended, checking mode...");

    switch (play_mode) {
        case PlayMode::SINGLE:
            EVENT_LOG("Playlist ended (SINGLE mode)");
            LOG("[AUTOPLAY] SINGLE mode: playback ended");
            break;

        case PlayMode::CYCLE:
            if (!current_playlist.empty()) {
                EVENT_LOG("Cycle: restarting playlist");
                LOG("[AUTOPLAY] CYCLE mode: restarting");
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
            if (!current_playlist.empty()) {
                EVENT_LOG("Random: reshuffling playlist");
                LOG("[AUTOPLAY] RANDOM mode: reshuffling");

                std::random_device rd;
                std::mt19937 gen(rd());
                std::vector<size_t> indices;
                for (size_t i = 0; i < current_playlist.size(); ++i) {
                    indices.push_back(i);
                }
                std::shuffle(indices.begin(), indices.end(), gen);

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

void App::clear_playlist() {
    int count = current_playlist.size();
    current_playlist.clear();
    current_playlist_index = -1;
    EVENT_LOG(fmt::format("Cleared playlist ({} items)", count));
    LOG(fmt::format("[PLAYLIST] Cleared {} items, current playback continues", count));
}

// =========================================================
// List mode (playlist management popup)
// =========================================================

void App::load_saved_playlist() {
    saved_playlist = DatabaseManager::instance().load_playlist();
    if (list_selected_idx >= static_cast<int>(saved_playlist.size())) {
        list_selected_idx = std::max(0, static_cast<int>(saved_playlist.size()) - 1);
    }
    LOG(fmt::format("[LIST] Loaded {} saved playlist items", saved_playlist.size()));
}

void App::save_current_playlist_to_db() {
    DatabaseManager::instance().clear_playlist_table();

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

    load_saved_playlist();
}

void App::add_to_saved_playlist(TreeNodePtr node) {
    if (!node) return;

    SavedPlaylistItem item;
    item.title = node->title;
    item.url = node->url;
    item.duration = node->duration;
    URLType url_type = URLClassifier::classify(node->url);
    item.is_video = node->is_youtube || URLClassifier::is_video(url_type);
    item.node_path = node->url;
    item.sort_order = static_cast<int>(saved_playlist.size());

    DatabaseManager::instance().save_playlist_item(item);
    saved_playlist.push_back(item);

    EVENT_LOG(fmt::format("Added to playlist: {}", node->title));
    LOG(fmt::format("[LIST] Added: {}", node->url));
}

void App::delete_from_saved_playlist(int idx) {
    if (idx < 0 || idx >= static_cast<int>(saved_playlist.size())) return;

    std::string title = saved_playlist[idx].title;
    int id = saved_playlist[idx].id;

    DatabaseManager::instance().delete_playlist_item(id);
    saved_playlist.erase(saved_playlist.begin() + idx);

    DatabaseManager::instance().reorder_playlist();
    load_saved_playlist();

    if (list_selected_idx >= static_cast<int>(saved_playlist.size())) {
        list_selected_idx = std::max(0, static_cast<int>(saved_playlist.size()) - 1);
    }

    EVENT_LOG(fmt::format("Removed from playlist: {}", title));
    LOG(fmt::format("[LIST] Deleted item at index {}", idx));
}

void App::clear_saved_playlist() {
    int count = saved_playlist.size();
    DatabaseManager::instance().clear_playlist_table();
    saved_playlist.clear();
    list_selected_idx = 0;
    EVENT_LOG(fmt::format("Cleared saved playlist ({} items)", count));
    LOG("[LIST] Cleared all saved playlist items");
}

void App::move_playlist_item_up(int idx) {
    if (idx <= 0 || idx >= static_cast<int>(saved_playlist.size())) return;

    int id1 = saved_playlist[idx].id;
    int id2 = saved_playlist[idx - 1].id;
    int order1 = saved_playlist[idx].sort_order;
    int order2 = saved_playlist[idx - 1].sort_order;

    DatabaseManager::instance().update_playlist_order(id1, order2);
    DatabaseManager::instance().update_playlist_order(id2, order1);

    load_saved_playlist();
    list_selected_idx = idx - 1;

    EVENT_LOG("Moved playlist item up");
}

void App::move_playlist_item_down(int idx) {
    if (idx < 0 || idx >= static_cast<int>(saved_playlist.size()) - 1) return;

    int id1 = saved_playlist[idx].id;
    int id2 = saved_playlist[idx + 1].id;
    int order1 = saved_playlist[idx].sort_order;
    int order2 = saved_playlist[idx + 1].sort_order;

    DatabaseManager::instance().update_playlist_order(id1, order2);
    DatabaseManager::instance().update_playlist_order(id2, order1);

    load_saved_playlist();
    list_selected_idx = idx + 1;

    EVENT_LOG("Moved playlist item down");
}

// =========================================================
// List mode playback (modular architecture)
// =========================================================

void App::play_saved_playlist_item(int idx) {
    if (idx < 0 || idx >= static_cast<int>(saved_playlist.size())) return;

    const auto& item = saved_playlist[idx];

    if (item.url.empty()) {
        EVENT_LOG("Cannot play: URL is empty");
        return;
    }

    // 1. Build complete playlist
    std::vector<std::string> playlist_urls;
    current_playlist.clear();
    bool has_video = false;

    for (size_t i = 0; i < saved_playlist.size(); ++i) {
        const auto& si = saved_playlist[i];

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

    // 2. Apply play mode processing
    auto processed = apply_play_mode_for_list(playlist_urls, idx, play_mode);

    // 3. Set current_playlist_index based on play mode
    switch (play_mode) {
        case PlayMode::SINGLE:
            current_playlist_index = idx;
            list_selected_idx = idx;
            shuffle_map_.clear();
            break;
        case PlayMode::CYCLE:
            current_playlist_index = idx;
            list_selected_idx = idx;
            shuffle_map_.clear();
            break;
        case PlayMode::RANDOM:
            if (!shuffle_map_.empty() && current_playlist_index >= 0) {
                list_selected_idx = current_playlist_index;
            }
            break;
    }

    // 4. Execute playback
    execute_play_for_list(processed.urls, processed.start_idx, has_video, processed.loop_single);

    playback_node = nullptr;

    EVENT_LOG(fmt::format("L-List Play: idx {}, cursor synced", idx));
}

App::ProcessedPlaylist App::apply_play_mode_for_list(const std::vector<std::string>& urls, int current_idx, PlayMode mode) {
    ProcessedPlaylist result;
    result.loop_single = false;

    if (urls.empty()) {
        result.start_idx = 0;
        return result;
    }

    if (current_idx < 0) current_idx = 0;
    if (current_idx >= static_cast<int>(urls.size())) current_idx = urls.size() - 1;

    switch (mode) {
        case PlayMode::SINGLE: {
            result.urls.push_back(urls[current_idx]);
            result.start_idx = 0;
            result.loop_single = true;
            EVENT_LOG(fmt::format("List Mode R (Repeat): single track loop - '{}'",
                current_idx < static_cast<int>(current_playlist.size()) ?
                current_playlist[current_idx].title : "unknown"));
            break;
        }

        case PlayMode::CYCLE: {
            result.urls = urls;
            result.start_idx = current_idx;
            EVENT_LOG(fmt::format("List Mode C (Cycle): {} items total, start from idx {}",
                result.urls.size(), current_idx));
            break;
        }

        case PlayMode::RANDOM: {
            std::vector<size_t> indices;
            for (size_t i = 0; i < urls.size(); ++i) {
                indices.push_back(i);
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(indices.begin(), indices.end(), gen);

            shuffle_map_.clear();
            shuffle_map_.resize(indices.size());
            for (size_t mpv_pos = 0; mpv_pos < indices.size(); ++mpv_pos) {
                shuffle_map_[mpv_pos] = indices[mpv_pos];
            }

            for (size_t i : indices) {
                result.urls.push_back(urls[i]);
            }
            result.start_idx = 0;

            current_playlist_index = static_cast<int>(shuffle_map_[0]);

            EVENT_LOG(fmt::format("List Mode Y (Random): {} items, first playing idx {}",
                result.urls.size(), current_playlist_index));
            break;
        }
    }

    return result;
}

void App::execute_play_for_list(const std::vector<std::string>& urls, int start_idx, bool has_video, bool loop_single) {
    if (urls.empty()) return;

    if (loop_single) {
        player.set_loop_file(true);
        player.set_loop_playlist(false);
    } else {
        player.set_loop_file(false);
    }

    player.play_list_from(urls, start_idx, has_video);

    LOG(fmt::format("[LIST] Executed: {} items, start_idx={}, loop_single={}",
        urls.size(), start_idx, loop_single));
}

// =========================================================
// Cache marking
// =========================================================

void App::mark_cached_nodes(TreeNodePtr node) {
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

// =========================================================
// handle_input - Keyboard input dispatcher
// =========================================================

void App::handle_input(int ch, int marked_count) {
    // List mode popup full key handling
    if (in_list_mode_) {
        int visible_lines = std::min(23, static_cast<int>(saved_playlist.size()));

        switch (ch) {
            // Navigation keys
            case 'j':
                if (list_selected_idx < static_cast<int>(saved_playlist.size()) - 1) {
                    list_selected_idx++;
                }
                break;
            case 'k':
                if (list_selected_idx > 0) {
                    list_selected_idx--;
                }
                break;
            case 'g':
                list_selected_idx = 0;
                break;
            case 'G':
                if (!saved_playlist.empty()) {
                    list_selected_idx = static_cast<int>(saved_playlist.size()) - 1;
                }
                break;
            case KEY_PPAGE:
            case 2:  // Ctrl+B
                list_selected_idx = std::max(0, list_selected_idx - visible_lines);
                break;
            case KEY_NPAGE:
            case 6:  // Ctrl+F
                list_selected_idx = std::min(static_cast<int>(saved_playlist.size()) - 1,
                                             list_selected_idx + visible_lines);
                break;

            // Edit operations
            case 'd':
                if (list_visual_mode_) {
                    std::vector<int> to_delete(list_marked_indices.begin(), list_marked_indices.end());
                    std::sort(to_delete.rbegin(), to_delete.rend());
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
            case 'J':
                move_playlist_item_down(list_selected_idx);
                break;
            case 'K':
                move_playlist_item_up(list_selected_idx);
                break;
            case 'C':
                if (ui.confirm_box("Clear entire playlist?")) {
                    clear_saved_playlist();
                }
                break;

            // Play mode toggle (only effective in L popup)
            case 'r':
            case 'R':
                play_mode = PlayMode::SINGLE;
                EVENT_LOG("Play mode: Single");
                break;
            case 'c':
                play_mode = PlayMode::CYCLE;
                EVENT_LOG("Play mode: Cycle");
                break;
            case 'y':
            case 'Y':
                play_mode = PlayMode::RANDOM;
                EVENT_LOG("Play mode: Random");
                break;

            // Mark and Visual mode
            case 'm':
                if (list_marked_indices.count(list_selected_idx)) {
                    list_marked_indices.erase(list_selected_idx);
                } else {
                    list_marked_indices.insert(list_selected_idx);
                }
                if (list_selected_idx < static_cast<int>(saved_playlist.size()) - 1) {
                    list_selected_idx++;
                }
                break;
            case 'v':
                if (!list_visual_mode_) {
                    list_visual_mode_ = true;
                    list_visual_start_ = list_selected_idx;
                    list_marked_indices.insert(list_selected_idx);
                } else {
                    list_visual_mode_ = false;
                }
                break;
            case 'V':
                list_visual_mode_ = false;
                list_marked_indices.clear();
                EVENT_LOG("Cleared all marks");
                break;

            // Playback operations
            case '\n':
            case 'l':
                play_saved_playlist_item(list_selected_idx);
                in_list_mode_ = false;
                break;
            case ' ':
                player.toggle_pause();
                break;

            // Volume and speed controls
            case '+':
                player.set_volume(player.get_state().volume + VOLUME_STEP);
                break;
            case '=': {
                int count = 0;
                if (!list_marked_indices.empty()) {
                    for (int idx : list_marked_indices) {
                        if (idx >= 0 && idx < static_cast<int>(saved_playlist.size())) {
                            SavedPlaylistItem new_item = saved_playlist[idx];
                            new_item.id = 0;
                            new_item.sort_order = static_cast<int>(saved_playlist.size());
                            DatabaseManager::instance().save_playlist_item(new_item);
                            saved_playlist.push_back(new_item);
                            count++;
                        }
                    }
                    list_marked_indices.clear();
                    list_visual_mode_ = false;
                } else if (list_selected_idx >= 0 && list_selected_idx < static_cast<int>(saved_playlist.size())) {
                    SavedPlaylistItem new_item = saved_playlist[list_selected_idx];
                    new_item.id = 0;
                    new_item.sort_order = static_cast<int>(saved_playlist.size());
                    DatabaseManager::instance().save_playlist_item(new_item);
                    saved_playlist.push_back(new_item);
                    count = 1;
                }
                if (count > 0) {
                    EVENT_LOG(fmt::format("= Added {} items to L-list", count));
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

            // Exit
            case 'o':
                toggle_saved_playlist_sort();
                break;
            case 'L':
            case 27:  // ESC
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
        return;  // List mode does not process other keys
    }

    // Help display handled directly
    if (visual_mode_) {
        if (ch == 'j') nav_down();
        else if (ch == 'k') nav_up();
        else if (ch == 'v') confirm_visual_selection();
        else if (ch == 'V') { visual_mode_ = false; clear_all_marks(); }
        else if (ch == 27) { visual_mode_ = false; EVENT_LOG("Visual cancelled"); }
        return;
    }

    switch (ch) {
        case 'q':
        case 'Q':
        case 27:
            if (ui.confirm_box("Quit PODRADIO?")) {
                running = false;
            }
            break;
        case '?': ui.show_help(player.get_state()); break;
        case 'R': mode = AppMode::RADIO; current_root = radio_root; reset_search(); selected_idx = 0; break;
        case 'P': mode = AppMode::PODCAST; current_root = podcast_root; reset_search(); selected_idx = 0; break;
        case 'F': mode = AppMode::FAVOURITE; current_root = fav_root; reset_search(); selected_idx = 0; break;
        case 'H': mode = AppMode::HISTORY; current_root = history_root; reset_search(); selected_idx = 0; break;
        case 'O': {
            mode = AppMode::ONLINE;
            current_root = OnlineState::instance().online_root;
            OnlineState::instance().load_search_history();
            reset_search();
            selected_idx = 0;
            EVENT_LOG("Switched to ONLINE mode - press '/' to search");
            break;
        }
        case 'M': {
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
        case 'B': {
            if (mode == AppMode::ONLINE) {
                std::string new_region = OnlineState::instance().get_next_region();
                EVENT_LOG(fmt::format("Search region: {}", ITunesSearch::get_region_name(new_region)));
            }
            break;
        }
        case 'r': reset_search(); refresh_node(); break;
        case 'C': clear_playlist(); break;
        case 'k': nav_up(); break;
        case 'j': nav_down(); break;
        case 'l':
        case '\n': {
            if (marked_count > 0) {
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
                    EVENT_LOG(fmt::format("+ Added {} items to playlist", count));
                }
            } else {
                enter_node(marked_count);
            }
            break;
        }
        case 'h': go_back(); break;
        case ' ': player.toggle_pause(); break;
        case '+':
            player.set_volume(player.get_state().volume + VOLUME_STEP);
            break;
        case '=': {
            int count = 0;
            if (marked_count > 0) {
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
                auto node = display_list[selected_idx].node;
                if (node && (node->type == NodeType::PODCAST_EPISODE ||
                             node->type == NodeType::RADIO_STREAM)) {
                    add_to_saved_playlist(node);
                    count = 1;
                }
            }
            if (count > 0) {
                EVENT_LOG(fmt::format("= Added {} items to L-list", count));
            }
            break;
        }
        case '-': player.set_volume(player.get_state().volume - VOLUME_STEP); break;
        case ']': player.adjust_speed(true); break;
        case '[': player.adjust_speed(false); break;
        case KEY_BACKSPACE:
        case '\\': player.reset_speed(); break;
        case 'g': nav_top(); break;
        case 'G': nav_bottom(); break;
        case KEY_PPAGE: nav_page_up(); break;
        case KEY_NPAGE: nav_page_down(); break;
        case KEY_RESIZE:
            break;
        case 'a': {
            if (mode == AppMode::PODCAST) {
                add_feed();
            } else if (mode == AppMode::ONLINE) {
                if (marked_count > 0) {
                    subscribe_online_podcasts_batch(marked_count);
                } else {
                    subscribe_online_podcast();
                }
            } else if (mode == AppMode::FAVOURITE) {
                if (marked_count > 0) {
                    subscribe_favourites_batch(marked_count);
                } else {
                    subscribe_favourite_single();
                }
            }
            break;
        }
        case 'e': edit_node(); break;
        case 'f': {
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
        case 'V': clear_all_marks(); break;
        case 'T': ui.toggle_tree_lines(); break;
        case 'S': ui.toggle_scroll_mode(); break;
        case 'U': {
            IconManager::toggle_style();
            EVENT_LOG(fmt::format("Icon style: {}", IconManager::get_style_name()));
            break;
        }
        case 'L':
            if (in_list_mode_) {
                in_list_mode_ = false;
                EVENT_LOG("Exit List Mode");
            } else {
                load_saved_playlist();
                in_list_mode_ = true;
                auto play_state = player.get_state();
                if (play_state.has_media && !play_state.current_url.empty()) {
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
        case 12:  // Ctrl+L
            ui.toggle_theme();
            break;
        case 'o': toggle_sort_order(); break;
        case '/':
            if (mode == AppMode::ONLINE) {
                perform_online_search();
            } else if (mode == AppMode::FAVOURITE) {
                bool under_online_link = false;
                if (selected_idx < (int)display_list.size()) {
                    auto node = display_list[selected_idx].node;
                    for (auto& f : fav_root->children) {
                        if (f->is_link && f->url == "online_root") {
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

// =========================================================
// Online search
// =========================================================

void App::perform_online_search() {
    std::string query = ui.input_box("Search iTunes Podcasts");
    if (UI::is_input_cancelled(query)) {
        EVENT_LOG("Search cancelled");
        return;
    }
    if (query.empty()) return;

    OnlineState::instance().last_query = query;
    EVENT_LOG(fmt::format("Searching: '{}' in {}",
        query,
        ITunesSearch::get_region_name(OnlineState::instance().current_region)));

    auto results = ITunesSearch::instance().search(query, OnlineState::instance().current_region);

    OnlineState::instance().add_or_update_search_node(
        query, OnlineState::instance().current_region, results);

    selected_idx = 0;
    view_start = 0;

    EVENT_LOG(fmt::format("Found {} podcasts", results.size()));
}

void App::perform_online_search_from_favourite() {
    std::string query = ui.input_box("Search iTunes Podcasts (sync to ONLINE)");
    if (UI::is_input_cancelled(query)) {
        EVENT_LOG("Search cancelled");
        return;
    }
    if (query.empty()) return;

    OnlineState::instance().last_query = query;
    EVENT_LOG(fmt::format("Searching: '{}' in {}",
        query,
        ITunesSearch::get_region_name(OnlineState::instance().current_region)));

    auto results = ITunesSearch::instance().search(query, OnlineState::instance().current_region);

    OnlineState::instance().add_or_update_search_node(
        query, OnlineState::instance().current_region, results);

    // Clear FAVOURITE online_root LINK children so they re-sync on next expand
    for (auto& f : fav_root->children) {
        if (f->is_link && f->url == "online_root") {
            f->children_loaded = false;
            f->children.clear();
        }
    }

    // Expand and refresh current LINK node
    if (selected_idx < (int)display_list.size()) {
        auto node = display_list[selected_idx].node;
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
                    {
                        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                        f->children.clear();
                        for (auto& child : OnlineState::instance().online_root->children) {
                            child->parent = f;
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

    // Rebuild display_list
    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        display_list.clear();
        flatten(current_root, 0, true, -1);
    }

    selected_idx = 0;
    view_start = 0;

    EVENT_LOG(fmt::format("Found {} podcasts (synced to ONLINE)", results.size()));
}

void App::load_search_history_children(TreeNodePtr node) {
    if (!node || node->url.find("search:") != 0) return;

    // Parse URL format: search:region:query
    std::string search_id = node->url.substr(7);
    size_t colon_pos = search_id.find(':');
    if (colon_pos == std::string::npos) return;

    std::string region = search_id.substr(0, colon_pos);
    std::string query = search_id.substr(colon_pos + 1);

    // Load from database cache
    std::string cached = DatabaseManager::instance().load_search_cache(query, region);
    if (cached.empty()) {
        EVENT_LOG(fmt::format("[Online] No cache for '{}'", query));
        return;
    }

    try {
        json j = json::parse(cached);
        if (j.contains("results") && j["results"].is_array()) {
            node->children.clear();
            for (const auto& item : j["results"]) {
                auto child = ITunesSearch::instance().parse_result(item);
                if (child) {
                    child->is_db_cached = DatabaseManager::instance().is_podcast_cached(child->url);
                    child->parent = node;
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

// =========================================================
// Favourite cache loading
// =========================================================

bool App::load_favourite_children_from_cache(TreeNodePtr node) {
    if (!node || node->url.empty()) return false;

    std::string url = node->url;
    URLType url_type = URLClassifier::classify(url);

    // Podcast Feed / YouTube / Apple types
    if (url_type == URLType::RSS_PODCAST || url_type == URLType::YOUTUBE_RSS ||
        url_type == URLType::YOUTUBE_CHANNEL || url_type == URLType::APPLE_PODCAST ||
        url_type == URLType::YOUTUBE_PLAYLIST) {

        if (!DatabaseManager::instance().is_podcast_cached(url)) {
            EVENT_LOG(fmt::format("[Favourite] Podcast cache not found: {}", url));
            return false;
        }

        auto episodes = DatabaseManager::instance().load_episodes_from_cache(url);
        if (episodes.empty()) {
            EVENT_LOG(fmt::format("[Favourite] No episodes cached for: {}", url));
            return false;
        }

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
        EVENT_LOG(fmt::format("[Favourite] Loaded {} episodes from cache", node->children.size()));
        return true;
    }

    // OPML/radio directory types
    if (url_type == URLType::OPML ||
        url.find(".opml") != std::string::npos ||
        url.find("Browse.ashx") != std::string::npos ||
        url.find("Tune.ashx") != std::string::npos) {

        if (DatabaseManager::instance().is_episode_cached(url)) {
            auto episodes = DatabaseManager::instance().load_episodes_from_cache(url);
            if (!episodes.empty()) {
                node->children.clear();
                for (const auto& [ep_url, ep_title, ep_duration, ep_data] : episodes) {
                    auto child = std::make_shared<TreeNode>();
                    child->title = ep_title.empty() ? "Untitled" : ep_title;
                    child->url = ep_url;
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

        EVENT_LOG(fmt::format("[Favourite] OPML cache not found: {}", url));
        return false;
    }

    // Generic fallback
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

// =========================================================
// Subscribe operations
// =========================================================

void App::subscribe_online_podcast() {
    if (selected_idx >= (int)display_list.size()) return;
    auto node = display_list[selected_idx].node;
    if (!node || node->url.empty()) return;

    for (const auto& child : podcast_root->children) {
        if (child->url == node->url) {
            EVENT_LOG("Already subscribed");
            return;
        }
    }

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

void App::subscribe_online_podcasts_batch(int /*marked_count*/) {
    std::vector<TreeNodePtr> to_subscribe;
    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        collect_marked(OnlineState::instance().online_root, to_subscribe);
    }

    if (to_subscribe.empty()) {
        EVENT_LOG("No items to subscribe");
        return;
    }

    std::vector<TreeNodePtr> feed_nodes;
    for (auto& n : to_subscribe) {
        if (n->type == NodeType::PODCAST_FEED && !n->url.empty()) {
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

    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        clear_marks(OnlineState::instance().online_root);
    }

    Persistence::save_cache(radio_root, podcast_root);
    Persistence::save_data(podcast_root->children, fav_root->children);

    EVENT_LOG(fmt::format("Subscribed {} podcasts", count));
}

void App::subscribe_favourite_single() {
    if (selected_idx >= (int)display_list.size()) return;
    auto node = display_list[selected_idx].node;
    if (!node || node->url.empty()) return;

    for (const auto& child : podcast_root->children) {
        if (child->url == node->url) {
            EVENT_LOG("Already subscribed");
            return;
        }
    }

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

void App::subscribe_favourites_batch(int /*marked_count*/) {
    std::vector<TreeNodePtr> to_subscribe;
    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        collect_marked(fav_root, to_subscribe);
    }

    if (to_subscribe.empty()) {
        EVENT_LOG("No items to subscribe");
        return;
    }

    std::vector<TreeNodePtr> feed_nodes;
    for (auto& n : to_subscribe) {
        if (!n->url.empty() && n->url != "online_root") {
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

    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        clear_marks(fav_root);
    }

    Persistence::save_cache(radio_root, podcast_root);
    Persistence::save_data(podcast_root->children, fav_root->children);

    EVENT_LOG(fmt::format("Subscribed {} podcasts from favourites", count));
}

// =========================================================
// Favourites
// =========================================================

void App::add_favourite() {
    if (selected_idx >= (int)display_list.size()) return;
    auto node = display_list[selected_idx].node;

    // Check for duplicate
    for (auto& f : fav_root->children) {
        if (!node->url.empty() && f->url == node->url) {
            EVENT_LOG(fmt::format("* Already in favourites: {}", node->title));
            return;
        }
        if (node->url.empty() && f->title == node->title && f->type == node->type) {
            EVENT_LOG(fmt::format("* Already in favourites: {}", node->title));
            return;
        }
    }

    std::string source_mode_name;
    switch (mode) {
        case AppMode::RADIO: source_mode_name = "RADIO"; break;
        case AppMode::PODCAST: source_mode_name = "PODCAST"; break;
        case AppMode::ONLINE: source_mode_name = "ONLINE"; break;
        case AppMode::FAVOURITE: source_mode_name = "FAVOURITE"; break;
        case AppMode::HISTORY: source_mode_name = "HISTORY"; break;
    }

    // Create LINK node
    auto fn = std::make_shared<TreeNode>();
    fn->title = node->title;
    fn->url = node->url;
    fn->type = node->type;
    fn->is_link = true;
    fn->link_target_url = node->url;
    fn->source_mode = source_mode_name;
    fn->linked_node = node;
    fn->is_youtube = node->is_youtube;
    fn->channel_name = node->channel_name;
    fn->children_loaded = false;

    // Special handling for Online Search root node
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

    json link_info;
    link_info["is_link"] = true;
    link_info["source_mode"] = source_mode_name;
    link_info["link_target_url"] = fn->link_target_url;
    std::string data_json = link_info.dump();

    DatabaseManager::instance().save_favourite(
        fn->title, fn->url, (int)fn->type,
        fn->is_youtube, fn->channel_name, source_mode_name, data_json
    );

    EVENT_LOG(fmt::format("* Favourite LINK: {} [{}]", node->title, source_mode_name));
}

void App::add_favourites_batch(int marked_count) {
    // ONLINE mode special handling
    if (mode == AppMode::ONLINE) {
        for (auto& f : fav_root->children) {
            if (f->url == "online_root") {
                EVENT_LOG("* Online Search already in favourites");
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                clear_marks(current_root);
                return;
            }
        }

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

        EVENT_LOG("* Favourite: Online Search (LINK to online_root)");
        return;
    }

    // Non-ONLINE mode batch favourites
    std::vector<TreeNodePtr> to_fav;
    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        collect_marked(current_root, to_fav);
    }

    if (to_fav.empty()) {
        EVENT_LOG("No items to favourite");
        return;
    }

    std::vector<TreeNodePtr> fav_nodes;
    std::set<std::string> existing_urls;
    for (const auto& f : fav_root->children) {
        if (!f->url.empty()) existing_urls.insert(f->url);
    }

    for (auto& n : to_fav) {
        if (n->type == NodeType::PODCAST_FEED ||
            n->type == NodeType::FOLDER ||
            n->type == NodeType::RADIO_STREAM ||
            n->type == NodeType::PODCAST_EPISODE) {
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

    std::string source_mode_name;
    switch (mode) {
        case AppMode::RADIO: source_mode_name = "RADIO"; break;
        case AppMode::PODCAST: source_mode_name = "PODCAST"; break;
        case AppMode::ONLINE: source_mode_name = "ONLINE"; break;
        case AppMode::FAVOURITE: source_mode_name = "FAVOURITE"; break;
        case AppMode::HISTORY: source_mode_name = "HISTORY"; break;
    }

    int count = 0;
    for (auto& n : fav_nodes) {
        auto fn = std::make_shared<TreeNode>();
        fn->title = n->title;
        fn->url = n->url;
        fn->type = n->type;
        fn->is_link = true;
        fn->link_target_url = n->url;
        fn->source_mode = source_mode_name;
        fn->linked_node = n;
        fn->is_youtube = n->is_youtube;
        fn->channel_name = n->channel_name;
        fn->children_loaded = false;

        fn->parent = fav_root;
        fav_root->children.insert(fav_root->children.begin(), fn);

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

    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        clear_marks(current_root);
    }

    EVENT_LOG(fmt::format("* Added {} favourites", count));
}

void App::clear_all_marks() {
    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
    clear_marks(current_root);
    EVENT_LOG("All marks cleared");
    Persistence::save_cache(radio_root, podcast_root);
}

void App::confirm_visual_selection() {
    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
    int start = std::min(visual_start_, selected_idx);
    int end = std::max(visual_start_, selected_idx);
    int count = 0;

    for (int i = start; i <= end && i < (int)display_list.size(); ++i) {
        auto node = display_list[i].node;
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

// =========================================================
// Navigation
// =========================================================

void App::nav_up() { if (selected_idx > 0) selected_idx--; }
void App::nav_down() { if (selected_idx < (int)display_list.size() - 1) selected_idx++; }
void App::nav_top() { selected_idx = 0; view_start = 0; }
void App::nav_bottom() { selected_idx = display_list.size() - 1; }
void App::nav_page_up() { selected_idx -= PAGE_SCROLL_LINES; if (selected_idx < 0) selected_idx = 0; }
void App::nav_page_down() { selected_idx += PAGE_SCROLL_LINES; if (selected_idx >= (int)display_list.size()) selected_idx = display_list.size() - 1; }

void App::go_back() {
    if (selected_idx >= (int)display_list.size()) return;
    auto node = display_list[selected_idx].node;

    // If the current node is expandable and expanded, collapse first
    if ((node->type == NodeType::FOLDER || node->type == NodeType::PODCAST_FEED) && node->expanded) {
        node->expanded = false;
        return;
    }

    // Try to jump to parent node
    int parent = display_list[selected_idx].parent_idx;
    if (parent != -1) {
        selected_idx = parent;
    } else {
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

// =========================================================
// LINK mechanism (favourites as soft links)
// =========================================================

TreeNodePtr App::get_root_by_mode_string(const std::string& mode_str) {
    if (mode_str == "RADIO") return radio_root;
    if (mode_str == "PODCAST") return podcast_root;
    if (mode_str == "ONLINE") return OnlineState::instance().online_root;
    if (mode_str == "FAVOURITE") return fav_root;
    if (mode_str == "HISTORY") return history_root;
    return nullptr;
}

bool App::should_use_radio_loader(const std::string& source_mode, NodeType node_type, const std::string& url) {
    if (source_mode == "RADIO") {
        return true;
    }
    if (source_mode == "PODCAST") {
        return false;
    }

    if (source_mode == "ONLINE") {
        if (node_type == NodeType::FOLDER) {
            return true;
        }
        return false;
    }

    URLType url_type = URLClassifier::classify(url);
    if (url_type == URLType::OPML ||
        url.find(".opml") != std::string::npos ||
        url.find("Browse.ashx") != std::string::npos ||
        url.find("Tune.ashx") != std::string::npos) {
        return true;
    }

    if (node_type == NodeType::FOLDER) {
        return true;
    }

    return false;
}

void App::sync_link_node_status(TreeNodePtr target) {
    if (!target || !target->children_loaded) return;

    std::lock_guard<std::recursive_mutex> lock(tree_mutex);

    std::function<void(TreeNodePtr)> sync_children = [&](TreeNodePtr parent) {
        for (auto& child : parent->children) {
            if (child->is_link) {
                auto linked = child->linked_node.lock();
                if (linked.get() == target.get()) {
                    child->loading = false;
                    child->children_loaded = true;
                    child->parse_failed = target->parse_failed;
                    child->error_msg = target->error_msg;

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
            sync_children(child);
        }
    };

    sync_children(fav_root);
    sync_children(podcast_root);
}

bool App::expand_link_node(TreeNodePtr node) {
    if (!node || !node->is_link) return false;

    std::string target_url = node->link_target_url.empty() ? node->url : node->link_target_url;

    TreeNodePtr early_target = node->linked_node.lock();
    if (early_target && early_target->children_loaded && !early_target->children.empty()) {
        node->loading = false;
    }

    // Step 1: Find target node
    TreeNodePtr target = node->linked_node.lock();

    if (!target) {
        TreeNodePtr search_root = get_root_by_mode_string(node->source_mode);

        if (target_url == "online_root") {
            if (!OnlineState::instance().history_loaded) {
                OnlineState::instance().load_search_history();
            }
            target = OnlineState::instance().online_root;
        }
        else if (target_url.find("search:") == 0) {
            load_search_history_children(node);
            node->expanded = !node->children.empty();
            node->loading = false;
            EVENT_LOG(fmt::format("[LINK] Loaded search record: {}", target_url));
            return !node->children.empty();
        }
        else if (search_root) {
            if (node->source_mode == "ONLINE") {
                if (!OnlineState::instance().history_loaded) {
                    OnlineState::instance().load_search_history();
                }
            }
            target = find_node_by_url(search_root, target_url);
        }
    }

    // Step 2: If target found, check if it needs loading
    if (target) {
        node->linked_node = target;

        if (target->children_loaded && !target->children.empty()) {
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
            node->loading = false;
            EVENT_LOG(fmt::format("[LINK] Synced from target: {} items", node->children.size()));
            return true;
        }

        EVENT_LOG(fmt::format("[LINK] Target not loaded, loading on target: {} (source_mode={}, type={})",
            target->title, node->source_mode, (int)target->type));

        if (should_use_radio_loader(node->source_mode, target->type, target_url)) {
            spawn_load_radio(target);
            node->loading = true;
            EVENT_LOG(fmt::format("[LINK] Loading RADIO on target: {}", target_url));
            return true;
        }

        // Podcast type: load from database cache first
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
                node->loading = false;
                EVENT_LOG(fmt::format("[LINK] Loaded {} episodes from cache to target", episodes.size()));
                return true;
            }
        }

        spawn_load_feed(target);
        node->loading = true;
        EVENT_LOG(fmt::format("[LINK] Loading PODCAST on target: {}", target_url));
        return true;
    }

    // Step 3: Target not found, try creating it in original location
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
        node->loading = false;
        node->linked_node = OnlineState::instance().online_root;
        EVENT_LOG("[LINK] Loaded from online_root");
        return true;
    }

    TreeNodePtr new_target = std::make_shared<TreeNode>();
    new_target->title = node->title;
    new_target->url = target_url;
    new_target->type = node->type;
    new_target->is_youtube = node->is_youtube;
    new_target->channel_name = node->channel_name;

    TreeNodePtr target_root = get_root_by_mode_string(node->source_mode);
    if (target_root) {
        {
            std::lock_guard<std::recursive_mutex> lock(tree_mutex);
            new_target->parent = target_root;
            target_root->children.insert(target_root->children.begin(), new_target);
        }
        node->linked_node = new_target;

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

    EVENT_LOG(fmt::format("[LINK] Fallback: loading directly on LINK node: {}", target_url));

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

TreeNodePtr App::find_node_by_url(TreeNodePtr root, const std::string& url) {
    if (!root) return nullptr;
    if (root->url == url) return root;

    for (auto& child : root->children) {
        auto found = find_node_by_url(child, url);
        if (found) return found;
    }
    return nullptr;
}

// =========================================================
// enter_node - Expand folders/feeds or play media
// =========================================================

void App::enter_node(int marked_count) {
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

            for (auto& n : items) {
                std::string local = CacheManager::instance().get_local_file(n->url);
                if (!local.empty() && fs::exists(local)) {
                    urls.push_back("file://" + local);
                } else {
                    urls.push_back(n->url);
                }
            }

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
            EVENT_LOG(fmt::format("Play batch ({} items)", urls.size()));
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
            bool is_search_history_node = (node->url.find("search:") == 0);
            bool need_load = (!node->children_loaded || node->children.empty()) && !node->url.empty();

            if (need_load) {
                if (is_search_history_node) {
                    load_search_history_children(node);
                    node->expanded = true;
                } else if (mode == AppMode::RADIO) {
                    spawn_load_radio(node);
                } else if (mode == AppMode::ONLINE) {
                    if (node->type == NodeType::PODCAST_FEED &&
                        DatabaseManager::instance().is_episode_cached(node->url)) {
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
                            EVENT_LOG(fmt::format("[Online] Loaded {} episodes from cache", node->children.size()));
                        } else {
                            spawn_load_feed(node);
                        }
                    } else {
                        spawn_load_feed(node);
                    }
                } else if (mode == AppMode::FAVOURITE) {
                    if (node->is_link) {
                        expand_link_node(node);
                    } else if (node->url.find("search:") == 0) {
                        load_search_history_children(node);
                        node->expanded = !node->children.empty();
                    } else if (node->type == NodeType::PODCAST_FEED) {
                        if (load_favourite_children_from_cache(node)) {
                            node->expanded = true;
                        } else {
                            spawn_load_feed(node);
                        }
                    } else if (node->type == NodeType::FOLDER) {
                        URLType url_type = URLClassifier::classify(node->url);

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
                                if (url_type == URLType::OPML ||
                                    node->url.find(".opml") != std::string::npos ||
                                    node->url.find("Browse.ashx") != std::string::npos) {
                                    spawn_load_radio(node);
                                } else {
                                    spawn_load_feed(node);
                                }
                            }
                        } else {
                            if (url_type == URLType::OPML ||
                                node->url.find(".opml") != std::string::npos ||
                                node->url.find("Browse.ashx") != std::string::npos) {
                                spawn_load_radio(node);
                            } else {
                                spawn_load_feed(node);
                            }
                        }
                    } else {
                        if (load_favourite_children_from_cache(node)) {
                            node->expanded = true;
                        } else {
                            spawn_load_feed(node);
                        }
                    }
                } else if (mode == AppMode::PODCAST) {
                    if (node->type == NodeType::PODCAST_FEED &&
                        DatabaseManager::instance().is_episode_cached(node->url)) {
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
                    spawn_load_feed(node);
                }
            } else {
                node->expanded = true;
            }
        } else {
            if (node->is_link) {
                node->children_loaded = false;
                node->children.clear();
            }
            node->expanded = false;
        }
    } else if (!node->url.empty()) {
        // Playable node: execute playback decision table
        auto play_state = player.get_state();
        bool has_media = play_state.has_media;
        bool is_playing = has_media && !play_state.paused;
        bool is_paused = has_media && play_state.paused;
        bool is_same_as_playing = has_media && play_state.current_url == node->url;

        // PLAYING + different track: just add to playlist
        if (is_playing && !is_same_as_playing) {
            int existing_idx = find_url_in_saved_playlist(node->url);
            if (existing_idx < 0) {
                add_to_saved_playlist(node);
                EVENT_LOG(fmt::format("+ Added to playlist (while playing): {}", node->title));
            } else {
                EVENT_LOG(fmt::format("Already in playlist: {}", node->title));
            }
            return;
        }

        // PLAYING + same track: do nothing
        if (is_playing && is_same_as_playing) {
            EVENT_LOG(fmt::format("Already playing: {}", node->title));
            return;
        }

        // PAUSED + same track: resume
        if (is_paused && is_same_as_playing) {
            player.toggle_pause();
            EVENT_LOG(fmt::format("Resume playing: {}", node->title));
            return;
        }

        // PAUSED + different track: fill path (overwrite)
        // Not playing: normal fill path

        EVENT_LOG(fmt::format("Play: {}", node->title));
        record_play_history(node->url, node->title, node->duration);

        std::vector<std::string> playlist_urls;
        int current_idx = 0;
        bool has_video = false;

        if (saved_playlist.empty() || is_paused) {
            playlist_urls = build_playlist_from_siblings_and_save(current_idx, has_video, node);
            if (is_paused) {
                EVENT_LOG(fmt::format("L-List Re-Fill (PAUSED): {} items, playing idx {}", playlist_urls.size(), current_idx));
            } else {
                EVENT_LOG(fmt::format("L-List Auto-Fill: {} items, playing idx {}", playlist_urls.size(), current_idx));
            }
        } else {
            int existing_idx = find_url_in_saved_playlist(node->url);
            if (existing_idx >= 0) {
                current_idx = existing_idx;
                EVENT_LOG(fmt::format("L-List: Found at idx {}, playing", current_idx));
            } else {
                add_to_saved_playlist(node);
                current_idx = static_cast<int>(saved_playlist.size()) - 1;
                EVENT_LOG(fmt::format("L-List: Added to end, idx {}", current_idx));
            }

            playlist_urls.clear();
            current_playlist.clear();
            for (size_t i = 0; i < saved_playlist.size(); ++i) {
                const auto& item = saved_playlist[i];

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

        list_selected_idx = current_idx;

        auto processed = apply_play_mode(playlist_urls, current_idx, play_mode);

        execute_play(processed.urls, processed.start_idx, has_video, processed.loop_single);

        playback_node = node;
    }
}

// =========================================================
// Playlist construction
// =========================================================

int App::find_url_in_saved_playlist(const std::string& url) {
    for (size_t i = 0; i < saved_playlist.size(); ++i) {
        if (saved_playlist[i].url == url) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<std::string> App::build_playlist_from_siblings_and_save(int& current_idx, bool& has_video, TreeNodePtr current) {
    std::vector<std::string> urls;
    current_idx = 0;
    has_video = false;

    if (!current) return urls;

    auto parent = current->parent.lock();

    // F mode special handling: single favourite episode
    if (mode == AppMode::FAVOURITE && parent && parent->title == "Root") {
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

        EVENT_LOG(fmt::format("L-List: Single item saved"));
        return urls;
    }

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

    DatabaseManager::instance().clear_playlist_table();
    saved_playlist.clear();
    current_playlist.clear();

    bool reversed = parent->sort_reversed;
    int save_order = 0;
    int actual_idx = 0;

    if (!reversed) {
        for (size_t i = 0; i < siblings.size(); ++i) {
            auto& sib = siblings[i];
            if (!sib->url.empty()) {
                std::string local = CacheManager::instance().get_local_file(sib->url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : sib->url;
                urls.push_back(play_url);

                SavedPlaylistItem save_item;
                save_item.title = sib->title;
                save_item.url = sib->url;
                save_item.duration = sib->duration;
                URLType url_type = URLClassifier::classify(sib->url);
                save_item.is_video = sib->is_youtube || URLClassifier::is_video(url_type);
                save_item.sort_order = save_order;
                DatabaseManager::instance().save_playlist_item(save_item);
                saved_playlist.push_back(save_item);

                PlaylistItem pl_item;
                pl_item.title = sib->title;
                pl_item.url = play_url;
                pl_item.duration = sib->duration;
                pl_item.is_video = save_item.is_video;
                current_playlist.push_back(pl_item);

                if (pl_item.is_video) has_video = true;

                if (i == cursor_idx) {
                    actual_idx = save_order;
                }
                save_order++;
            }
        }
    } else {
        for (int i = static_cast<int>(siblings.size()) - 1; i >= 0; --i) {
            auto& sib = siblings[i];
            if (!sib->url.empty()) {
                std::string local = CacheManager::instance().get_local_file(sib->url);
                std::string play_url = (!local.empty() && fs::exists(local)) ?
                    "file://" + local : sib->url;
                urls.push_back(play_url);

                SavedPlaylistItem save_item;
                save_item.title = sib->title;
                save_item.url = sib->url;
                save_item.duration = sib->duration;
                URLType url_type = URLClassifier::classify(sib->url);
                save_item.is_video = sib->is_youtube || URLClassifier::is_video(url_type);
                save_item.sort_order = save_order;
                DatabaseManager::instance().save_playlist_item(save_item);
                saved_playlist.push_back(save_item);

                PlaylistItem pl_item;
                pl_item.title = sib->title;
                pl_item.url = play_url;
                pl_item.duration = sib->duration;
                pl_item.is_video = save_item.is_video;
                current_playlist.push_back(pl_item);

                if (pl_item.is_video) has_video = true;

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

std::vector<std::string> App::build_playlist_from_saved(int& current_idx, bool& has_video, TreeNodePtr current_node) {
    std::vector<std::string> urls;
    current_idx = 0;
    has_video = false;

    for (size_t i = 0; i < saved_playlist.size(); ++i) {
        if (saved_playlist[i].url == current_node->url ||
            saved_playlist[i].url.find(current_node->url) != std::string::npos ||
            current_node->url.find(saved_playlist[i].url) != std::string::npos) {
            current_idx = static_cast<int>(i);
            break;
        }
    }

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

std::vector<std::string> App::build_playlist_from_siblings(int& current_idx, bool& has_video, TreeNodePtr current) {
    std::vector<std::string> urls;
    current_idx = 0;
    has_video = false;

    if (!current) return urls;

    auto parent = current->parent.lock();

    // F mode special handling: single favourite episode
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

    current_playlist.clear();

    bool reversed = parent->sort_reversed;

    if (!reversed) {
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
        current_idx = static_cast<int>(current_playlist.size()) > 0 ?
            static_cast<int>(cursor_idx) : 0;
    } else {
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
        current_idx = static_cast<int>(current_playlist.size()) > 0 ?
            static_cast<int>(siblings.size() - 1 - cursor_idx) : 0;
    }

    current_playlist_index = current_idx;
    return urls;
}

// =========================================================
// Play mode processing and execution
// =========================================================

App::ProcessedPlaylist App::apply_play_mode(const std::vector<std::string>& urls, int current_idx, PlayMode mode) {
    ProcessedPlaylist result;
    result.loop_single = false;

    if (urls.empty()) {
        result.start_idx = 0;
        return result;
    }

    if (current_idx < 0) current_idx = 0;
    if (current_idx >= static_cast<int>(urls.size())) current_idx = urls.size() - 1;

    switch (mode) {
        case PlayMode::SINGLE: {
            result.urls.push_back(urls[current_idx]);
            result.start_idx = 0;
            result.loop_single = true;
            EVENT_LOG(fmt::format("Play Mode R (Repeat): single track loop - '{}'",
                current_idx < static_cast<int>(current_playlist.size()) ?
                current_playlist[current_idx].title : "unknown"));
            break;
        }

        case PlayMode::CYCLE: {
            for (size_t i = current_idx; i < urls.size(); ++i) {
                result.urls.push_back(urls[i]);
            }
            result.start_idx = 0;
            EVENT_LOG(fmt::format("Play Mode C (Cycle): {} items from idx {}",
                result.urls.size(), current_idx));
            break;
        }

        case PlayMode::RANDOM: {
            std::vector<size_t> indices;
            for (size_t i = 0; i < urls.size(); ++i) {
                indices.push_back(i);
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(indices.begin(), indices.end(), gen);

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

void App::execute_play(const std::vector<std::string>& urls, int start_idx, bool has_video, bool loop_single) {
    if (urls.empty()) return;

    if (urls.size() == 1) {
        player.play(urls[0], has_video);
        if (loop_single) {
            player.set_loop_file(true);
            EVENT_LOG("Execute: single track with loop");
        } else {
            EVENT_LOG("Execute: single track");
        }
    } else {
        player.play_list_from(urls, start_idx, has_video);
        EVENT_LOG(fmt::format("Execute: {} items, start from idx {}", urls.size(), start_idx));
    }

    LOG(fmt::format("[PLAY] Executed: {} items, start_idx={}, loop={}",
        urls.size(), start_idx, loop_single));
}

// =========================================================
// Toggle mark
// =========================================================

void App::toggle_mark() {
    if (selected_idx >= (int)display_list.size()) return;
    auto node = display_list[selected_idx].node;
    node->marked = !node->marked;
    EVENT_LOG(fmt::format("Mark: {}", node->marked ? "ON" : "OFF"));
    Persistence::save_cache(radio_root, podcast_root);
}

// =========================================================
// Sorting
// =========================================================

bool App::title_compare_asc(const std::string& a, const std::string& b) {
    bool a_starts_digit = !a.empty() && std::isdigit(static_cast<unsigned char>(a[0]));
    bool b_starts_digit = !b.empty() && std::isdigit(static_cast<unsigned char>(b[0]));

    if (a_starts_digit && !b_starts_digit) return true;
    if (!a_starts_digit && b_starts_digit) return false;

    return a < b;
}

bool App::title_compare_desc(const std::string& a, const std::string& b) {
    bool a_starts_digit = !a.empty() && std::isdigit(static_cast<unsigned char>(a[0]));
    bool b_starts_digit = !b.empty() && std::isdigit(static_cast<unsigned char>(b[0]));

    if (a_starts_digit && !b_starts_digit) return false;
    if (!a_starts_digit && b_starts_digit) return true;

    return a > b;
}

void App::toggle_saved_playlist_sort() {
    if (saved_playlist.empty()) {
        EVENT_LOG("Sort L-List: Empty list");
        return;
    }

    saved_playlist_sort_reversed_ = !saved_playlist_sort_reversed_;

    std::vector<size_t> indices(saved_playlist.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    if (saved_playlist_sort_reversed_) {
        std::sort(indices.begin(), indices.end(),
            [this](size_t a, size_t b) {
                return title_compare_desc(saved_playlist[a].title, saved_playlist[b].title);
            });
    } else {
        std::sort(indices.begin(), indices.end(),
            [this](size_t a, size_t b) {
                return title_compare_asc(saved_playlist[a].title, saved_playlist[b].title);
            });
    }

    std::vector<SavedPlaylistItem> new_playlist;
    new_playlist.reserve(saved_playlist.size());
    for (size_t idx : indices) {
        new_playlist.push_back(saved_playlist[idx]);
    }
    saved_playlist = std::move(new_playlist);

    DatabaseManager::instance().clear_playlist_table();
    for (size_t i = 0; i < saved_playlist.size(); ++i) {
        saved_playlist[i].sort_order = static_cast<int>(i);
        DatabaseManager::instance().save_playlist_item(saved_playlist[i]);
    }

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

    list_selected_idx = 0;

    EVENT_LOG(fmt::format("Sort L-List: {} ({} items)",
        saved_playlist_sort_reversed_ ? "Z->A" : "A->Z",
        saved_playlist.size()));
}

void App::toggle_sort_order() {
    if (selected_idx >= (int)display_list.size()) return;
    auto node = display_list[selected_idx].node;
    if (!node) return;

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

    sort_target->sort_reversed = !sort_target->sort_reversed;

    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        if (sort_target->sort_reversed) {
            std::sort(sort_target->children.begin(), sort_target->children.end(),
                [](const TreeNodePtr& a, const TreeNodePtr& b) {
                    return title_compare_desc(a->title, b->title);
                });
        } else {
            std::sort(sort_target->children.begin(), sort_target->children.end(),
                [](const TreeNodePtr& a, const TreeNodePtr& b) {
                    return title_compare_asc(a->title, b->title);
                });
        }

        display_list.clear();
        flatten(current_root, 0, true, -1);
    }

    std::string scope_desc;
    if (sort_target == current_root) {
        scope_desc = mode == AppMode::RADIO ? "Radio List" :
                     mode == AppMode::PODCAST ? "Podcast List" : "Current List";
    } else {
        scope_desc = sort_target->title.empty() ? "current list" : sort_target->title;
    }

    EVENT_LOG(fmt::format("Sort [{}]: {} ({} items)",
        scope_desc,
        sort_target->sort_reversed ? "Z->A" : "A->Z",
        sort_target->children.size()));

    Persistence::save_cache(radio_root, podcast_root);
}

// =========================================================
// Mark/collect helpers
// =========================================================

int App::count_marked_safe(const TreeNodePtr& node) {
    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
    return count_marked(node);
}

int App::count_marked(const TreeNodePtr& node) {
    int count = node->marked ? 1 : 0;
    for (auto& child : node->children) count += count_marked(child);
    return count;
}

void App::collect_playable_marked(const TreeNodePtr& node, std::vector<TreeNodePtr>& list) {
    if (node->marked) {
        if (node->type == NodeType::RADIO_STREAM || node->type == NodeType::PODCAST_EPISODE) {
            list.push_back(node);
        }
        else if (node->type == NodeType::FOLDER || node->type == NodeType::PODCAST_FEED) {
            for (auto& child : node->children) {
                collect_playable_items(child, list);
            }
        }
    }
    for (auto& child : node->children) {
        collect_playable_marked(child, list);
    }
}

void App::collect_playable_items(const TreeNodePtr& node, std::vector<TreeNodePtr>& list) {
    if (node->type == NodeType::RADIO_STREAM || node->type == NodeType::PODCAST_EPISODE) {
        list.push_back(node);
    }
    for (auto& child : node->children) {
        collect_playable_items(child, list);
    }
}

void App::clear_marks(const TreeNodePtr& node) {
    node->marked = false;
    for (auto& child : node->children) clear_marks(child);
}

void App::collect_marked(const TreeNodePtr& node, std::vector<TreeNodePtr>& list) {
    if (node->marked) list.push_back(node);
    for (auto& child : node->children) collect_marked(child, list);
}

bool App::remove_node(TreeNodePtr parent, TreeNodePtr target) {
    auto& children = parent->children;
    auto it = std::remove_if(children.begin(), children.end(), [&](auto& n) { return n == target; });
    if (it != children.end()) { children.erase(it, children.end()); return true; }
    for (auto& child : parent->children) if (remove_node(child, target)) return true;
    return false;
}

// =========================================================
// Search
// =========================================================

void App::reset_search() {
    search_query.clear();
    search_matches.clear();
    current_match_idx = -1;
    total_matches = 0;
}

void App::perform_search() {
    std::string q = ui.input_box("Search:");
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

void App::search_recursive(const TreeNodePtr& node, const std::string& query, std::vector<TreeNodePtr>& results) {
    if (Utils::to_lower(node->title).find(query) != std::string::npos)
        results.push_back(node);
    for (auto& child : node->children) search_recursive(child, query, results);
}

void App::jump_search(int dir) {
    if (total_matches == 0) return;
    current_match_idx = (current_match_idx + dir + total_matches) % total_matches;
    jump_to_match(current_match_idx);
}

void App::jump_to_match(int idx) {
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

void App::reveal_node(TreeNodePtr node) {
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

// =========================================================
// delete_node - Supports all modes, ONLINE enhanced, multi-select
// =========================================================

void App::delete_node(int marked_count) {
    if (selected_idx >= (int)display_list.size()) return;
    auto node = display_list[selected_idx].node;

    // ONLINE mode - supports multi-select delete
    if (mode == AppMode::ONLINE) {
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
                if (n->url.find("search:") == 0) {
                    std::string search_id = n->url.substr(7);
                    size_t pos = search_id.find(':');
                    if (pos != std::string::npos) {
                        std::string region = search_id.substr(0, pos);
                        std::string query = search_id.substr(pos + 1);
                        DatabaseManager::instance().delete_search_history(query, region);
                    }
                    for (auto& child : n->children) {
                        if (child->type == NodeType::PODCAST_FEED && !child->url.empty()) {
                            DatabaseManager::instance().delete_podcast_cache(child->url);
                            DatabaseManager::instance().delete_episode_cache_by_feed(child->url);
                        }
                    }
                } else if (n->type == NodeType::PODCAST_FEED && !n->url.empty()) {
                    DatabaseManager::instance().delete_podcast_cache(n->url);
                    DatabaseManager::instance().delete_episode_cache_by_feed(n->url);
                }

                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    remove_node(OnlineState::instance().online_root, n);
                }
                deleted_count++;
            }

            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                clear_marks(OnlineState::instance().online_root);
            }

            EVENT_LOG(fmt::format("Deleted {} items from ONLINE", deleted_count));
            if (selected_idx > 0) selected_idx--;
            return;
        }

        // Single delete in ONLINE mode
        if (node->url.find("search:") == 0) {
            std::string response = ui.dialog("Delete this search record and all caches? (Y/N)");
            if (response == "Y" || response == "y") {
                std::string search_id = node->url.substr(7);
                size_t pos = search_id.find(':');
                if (pos != std::string::npos) {
                    std::string region = search_id.substr(0, pos);
                    std::string query = search_id.substr(pos + 1);
                    DatabaseManager::instance().delete_search_history(query, region);
                }

                for (auto& child : node->children) {
                    if (child->type == NodeType::PODCAST_FEED && !child->url.empty()) {
                        DatabaseManager::instance().delete_podcast_cache(child->url);
                        DatabaseManager::instance().delete_episode_cache_by_feed(child->url);
                    }
                }

                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    remove_node(OnlineState::instance().online_root, node);
                }
                EVENT_LOG(fmt::format("Deleted search record and caches: {}", node->title));
                if (selected_idx > 0) selected_idx--;
            }
        } else if (node->type == NodeType::PODCAST_FEED) {
            std::string response = ui.dialog("Delete this podcast cache? (Y/N)");
            if (response == "Y" || response == "y") {
                if (!node->url.empty()) {
                    DatabaseManager::instance().delete_podcast_cache(node->url);
                    DatabaseManager::instance().delete_episode_cache_by_feed(node->url);
                }
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    remove_node(OnlineState::instance().online_root, node);
                }
                EVENT_LOG(fmt::format("Deleted podcast cache: {}", node->title));
                if (selected_idx > 0) selected_idx--;
            }
        } else {
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

    // HISTORY mode - supports multi-select delete
    if (mode == AppMode::HISTORY) {
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
                if (!n->url.empty()) {
                    DatabaseManager::instance().delete_history(n->url);
                }
                {
                    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                    remove_node(history_root, n);
                }
                deleted_count++;
            }

            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                clear_marks(history_root);
            }

            EVENT_LOG(fmt::format("Deleted {} history records", deleted_count));
            if (selected_idx > 0) selected_idx--;
            return;
        }

        std::string response = ui.dialog("Delete this history record? (Y/N)");
        if (response == "Y" || response == "y") {
            DatabaseManager::instance().delete_history(node->url);
            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                remove_node(history_root, node);
            }
            EVENT_LOG(fmt::format("Deleted history: {}", node->title));
            if (selected_idx > 0) selected_idx--;
        }
        return;
    }

    // PODCAST/FAVOURITE modes
    if (mode != AppMode::PODCAST && mode != AppMode::FAVOURITE) return;

    // FAVOURITE mode: handle LINK nodes specially
    if (mode == AppMode::FAVOURITE) {
        // Check if node is inside an online_root LINK
        TreeNodePtr parent_link;
        for (auto& f : fav_root->children) {
            if (f->is_link && f->url == "online_root") {
                if (f.get() == node.get()) {
                    // Deleting the LINK node itself
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
                for (auto& child : f->children) {
                    if (child.get() == node.get()) {
                        parent_link = f;
                        break;
                    }
                }
                if (parent_link) break;
            }
        }

        if (parent_link && parent_link->url == "online_root") {
            std::string response = ui.dialog("Delete from both FAVOURITE and ONLINE? (Y/N)");
            if (response != "Y" && response != "y") return;

            {
                std::lock_guard<std::recursive_mutex> lock(tree_mutex);
                remove_node(OnlineState::instance().online_root, node);
            }

            if (node->url.find("search:") == 0) {
                std::string search_id = node->url.substr(7);
                size_t pos = search_id.find(':');
                if (pos != std::string::npos) {
                    std::string region = search_id.substr(0, pos);
                    std::string query = search_id.substr(pos + 1);
                    DatabaseManager::instance().delete_search_history(query, region);
                }
            }

            parent_link->children_loaded = false;
            parent_link->children.clear();

            EVENT_LOG(fmt::format("Deleted from both: {}", node->title));
            if (selected_idx > 0) selected_idx--;
            return;
        }
    }

    // Multi-select batch delete
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

void App::clear_feed_cache(TreeNodePtr feed) {
    for (auto& child : feed->children) {
        CacheManager::instance().clear_cache(child->url);
        CacheManager::instance().clear_download(child->url);
        child->is_cached = false;
        child->is_downloaded = false;
        child->local_file.clear();
    }
    feed->is_cached = false;
    CacheManager::instance().clear_cache(feed->url);
}

// =========================================================
// download_node - Download media files
// =========================================================

void App::download_node(int marked_count) {
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
            // YouTube download with progress parsing
            std::thread t([this, url = n->url, base_name, dir, title = n->title, dl_id, n]() {
                EVENT_LOG(fmt::format("YouTube DL: {}", title));
                ProgressManager::instance().update(dl_id, 5, "Fetching info...", "...", 0, 0, 0);

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

                        if (line.find("[download]") != std::string::npos) {
                            size_t pct_pos = line.find('%');
                            if (pct_pos != std::string::npos) {
                                size_t num_start = pct_pos;
                                while (num_start > 0 && (isdigit(line[num_start-1]) || line[num_start-1] == '.')) {
                                    num_start--;
                                }
                                std::string pct_str = line.substr(num_start, pct_pos - num_start);
                                try {
                                    int percent = static_cast<int>(std::stod(pct_str));
                                    if (percent > 100) percent = 100;

                                    std::string speed = "...";
                                    size_t speed_pos = line.find(" at ");
                                    if (speed_pos != std::string::npos) {
                                        size_t speed_end = line.find(" ETA", speed_pos);
                                        if (speed_end != std::string::npos) {
                                            speed = line.substr(speed_pos + 4, speed_end - speed_pos - 4);
                                            while (!speed.empty() && isspace(speed.front())) speed.erase(0, 1);
                                            while (!speed.empty() && isspace(speed.back())) speed.pop_back();
                                        }
                                    }

                                    int eta_seconds = 0;
                                    size_t eta_pos = line.find("ETA");
                                    if (eta_pos != std::string::npos) {
                                        std::string eta_str = line.substr(eta_pos + 4);
                                        while (!eta_str.empty() && isspace(eta_str.front())) eta_str.erase(0, 1);
                                        while (!eta_str.empty() && (isspace(eta_str.back()) || eta_str.back() == '\n' || eta_str.back() == '\r')) {
                                            eta_str.pop_back();
                                        }
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
            // Regular download with progress callback
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
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
                    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);

                    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress_callback);
                    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
                    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

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

// =========================================================
// History
// =========================================================

void App::load_history_to_root() {
    auto history = DatabaseManager::instance().get_history(100);
    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
    history_root->children.clear();

    for (const auto& [url, title, timestamp] : history) {
        auto node = std::make_shared<TreeNode>();
        node->title = title.empty() ? "Unknown" : title;
        node->url = url;
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
        node->subtext = timestamp;
        history_root->children.push_back(node);
    }
}

void App::record_play_history(const std::string& url, const std::string& title, int duration) {
    if (url.empty()) return;
    DatabaseManager::instance().add_history(url, title, duration);
    load_history_to_root();
}

// =========================================================
// Edit node
// =========================================================

void App::edit_node() {
    if (selected_idx >= (int)display_list.size()) return;
    auto node = display_list[selected_idx].node;

    if (node->type != NodeType::PODCAST_FEED && node->type != NodeType::RADIO_STREAM) {
        EVENT_LOG("Can only edit feed/stream nodes");
        return;
    }

    std::string new_title = ui.input_box("Title:", node->title);
    if (new_title.empty()) new_title = node->title;

    std::string new_url = ui.input_box("URL:", node->url);
    if (new_url.empty()) new_url = node->url;

    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        node->title = new_title;
        node->url = new_url;
    }

    EVENT_LOG(fmt::format("Updated: {} -> {}", new_title, new_url.substr(0, 50)));
    Persistence::save_cache(radio_root, podcast_root);
}

// =========================================================
// Refresh node
// =========================================================

void App::refresh_node() {
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

// =========================================================
// Add feed
// =========================================================

void App::add_feed() {
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
    node->parse_failed = false;

    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        node->parent = podcast_root;
        podcast_root->children.insert(podcast_root->children.begin(), node);
    }

    Persistence::save_data(podcast_root->children, fav_root->children);
    EVENT_LOG(fmt::format("Subscription saved: {}", url));

    spawn_load_feed(node);
}

// =========================================================
// Persistence
// =========================================================

void App::load_persistent_data() {
    std::vector<TreeNodePtr> podcasts, favs;
    Persistence::load_data(podcasts, favs);
    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
    if (!podcast_root->children_loaded) {
        for (auto& n : podcasts) {
            n->parent = podcast_root;
            podcast_root->children.push_back(n);
        }
        podcast_root->children_loaded = true;
    }
    for (auto& n : favs) {
        n->parent = fav_root;
        fav_root->children.push_back(n);
    }
}

void App::save_persistent_data() {
    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
    Persistence::save_data(podcast_root->children, fav_root->children);
}

// =========================================================
// Restore player state
// =========================================================

void App::restore_player_state() {
    auto saved_state = DatabaseManager::instance().load_player_state();

    if (saved_state.volume >= 0 && saved_state.volume <= 100) {
        player.set_volume(saved_state.volume);
        EVENT_LOG(fmt::format("Restored volume: {}%", saved_state.volume));
    }

    if (saved_state.speed >= 0.5 && saved_state.speed <= 2.0) {
        player.set_speed(saved_state.speed);
        EVENT_LOG(fmt::format("Restored speed: {:.2f}x", saved_state.speed));
    }

    if (!saved_state.current_url.empty() && saved_state.position > 0) {
        EVENT_LOG(fmt::format("Last played: {} (position: {:.1f}s)",
            saved_state.current_url, saved_state.position));

        DatabaseManager::instance().save_progress(
            saved_state.current_url, saved_state.position, false);
    }

    ui.set_scroll_mode(saved_state.scroll_mode);
    ui.set_show_tree_lines(saved_state.show_tree_lines);
    EVENT_LOG(fmt::format("Restored UI: scroll_mode={}, tree_lines={}",
               saved_state.scroll_mode ? "ON" : "OFF",
               saved_state.show_tree_lines ? "ON" : "OFF"));

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

    if (!saved_state.current_title.empty()) {
        EVENT_LOG(fmt::format("Last played: {}", saved_state.current_title));
    }
}

// =========================================================
// Radio loading
// =========================================================

void App::load_radio_root() {
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

void App::spawn_load_radio(TreeNodePtr node, bool force) {
    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        if (node->loading) return;
        node->loading = true;
    }

    EVENT_LOG(fmt::format("Loading: [RADIO] {}", node->url));

    if (node->children_loaded && !force) {
        node->loading = false;
        node->expanded = true;
        return;
    }

    std::thread([this, node]() {
        std::string url = node->url;
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

        sync_link_node_status(node);
    }).detach();
}

void App::spawn_load_feed(TreeNodePtr node) {
    {
        std::lock_guard<std::recursive_mutex> lock(tree_mutex);
        if (node->loading) return;
        node->loading = true;
        node->parse_failed = false;
    }

    reset_xml_error_state();

    std::string url = node->url;
    URLType url_type = URLClassifier::classify(url);

    EVENT_LOG(fmt::format("Loading: [{}] {}", URLClassifier::type_name(url_type), url));

    if (url_type == URLType::APPLE_PODCAST) {
        std::string feed = Network::lookup_apple_feed(url);
        if (!feed.empty()) {
            url = feed;
            url_type = URLClassifier::classify(url);
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

        std::lock_guard<std::recursive_mutex> lock(tree_mutex);

        if (result) {
            if (!result->children.empty()) {
                node->children = result->children;
                node->children_loaded = true;
                node->expanded = true;
                node->parse_failed = false;
                node->is_cached = true;
                if (result->is_youtube) node->is_youtube = true;
                CacheManager::instance().mark_cached(node->url);
                EVENT_LOG(fmt::format("Loaded: {} items", result->children.size()));
                Persistence::save_cache(radio_root, podcast_root);
                Persistence::save_data(podcast_root->children, fav_root->children);

                // Save episode list to database cache
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
        node->loading = false;

        sync_link_node_status(node);
    }).detach();
}

// =========================================================
// load_default_podcasts - Built-in TOP200 podcast subscriptions
// =========================================================

void App::load_default_podcasts() {
    std::lock_guard<std::recursive_mutex> lock(tree_mutex);
    struct BuiltinPodcast { const char* name; const char* url; };

    std::set<std::string> existing_urls;
    for (const auto& child : podcast_root->children) {
        if (!child->url.empty()) {
            existing_urls.insert(child->url);
        }
    }

    std::vector<BuiltinPodcast> defaults = {
        // TOP1-20
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

        // TOP21-50
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

        // TOP51-100
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
        {"The Book Club", "https://podcasts.apple.com/us/podcast/the-book-club/id1876043295"},
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

        // TOP101-150
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

        // TOP151-200
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
        {"Mike Ward Sous Ecoute", "https://podcasts.apple.com/us/podcast/mike-ward/id1053196322"},
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

        // YouTube channel
        {"56BelowTV (YouTube)", "https://www.youtube.com/@56BelowTV"},
    };

    for (auto& p : defaults) {
        if (existing_urls.count(p.url)) continue;

        auto node = std::make_shared<TreeNode>();
        node->title = p.name;
        node->url = p.url;
        node->type = NodeType::PODCAST_FEED;
        node->children_loaded = false;
        node->expanded = false;
        node->parent = podcast_root;
        if (std::string(p.url).find("youtube.com") != std::string::npos) {
            node->is_youtube = true;
        }
        podcast_root->children.push_back(node);
    }
    podcast_root->children_loaded = true;
}

// =========================================================
// flatten - Convert tree to flat display list
// =========================================================

void App::flatten(const TreeNodePtr& node, int depth, bool is_last, int parent_idx) {
    int current_idx = display_list.size();
    if (node->title != "Root") {
        display_list.push_back({node, depth, is_last, parent_idx});
    }
    if ((node->type == NodeType::FOLDER || node->type == NodeType::PODCAST_FEED) &&
        (node->expanded || node->title == "Root")) {
        int count = node->children.size();
        for (int i = 0; i < count; ++i) {
            flatten(node->children[i], depth + (node->title != "Root" ? 1 : 0),
                   i == count - 1, current_idx);
        }
    }
}

} // namespace podradio
