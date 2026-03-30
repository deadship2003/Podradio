#pragma once

// =========================================================
// UI - Terminal user interface rendering
//
// Manages ncurses windows, drawing panels, status bar,
// input dialogs, help overlay, and list popup.
// =========================================================

#include "common.h"
#include "layout.h"
#include "mpv_controller.h"

namespace podradio
{

// =========================================================
// File-level helper functions for border drawing
// =========================================================

// Draw a complete rectangular border on a window, ensuring all four corners are closed
void draw_box(WINDOW* win);

// Force-redraw borders, overwriting any overflow content
// Parameters:
//   win - Window pointer
//   ww, wh - Window width and height
//   title_start, title_end - Title embedded area (column indices), -1 for none
//   split_y - Separator line row number, -1 for none
//   split_title_start, split_title_end - Separator line title area
void protect_border(WINDOW* win, int ww, int wh,
                    int title_start = -1, int title_end = -1,
                    int split_y = -1,
                    int split_title_start = -1, int split_title_end = -1);

// =========================================================
// UI class - Terminal rendering engine
// =========================================================
class UI {
public:
    // Special marker returned by input_box() when user cancels (ESC)
    static constexpr const char* INPUT_CANCELLED = "\x01" "CANCELLED" "\x01";

    // ---- Lifecycle ----

    // Initialize ncurses, create windows, load config and cache
    void init(float ratio = 0.4f);

    // Destroy windows and restore terminal state
    void cleanup();

    // ---- Toggle settings ----

    void toggle_tree_lines();
    void toggle_scroll_mode();
    void set_scroll_mode(bool mode);
    void set_show_tree_lines(bool show);
    bool is_scroll_mode() const;

    // ---- Main render ----

    // Main draw call - renders all panels based on current state
    void draw(AppMode mode, const std::vector<DisplayItem>& list, int selected,
              const MPVController::State& state, int view_start,
              AppState app_state, TreeNodePtr playback_node, int marked_count,
              const std::string& search_query, int current_match, int total_matches,
              TreeNodePtr selected_node, const std::vector<DownloadProgress>& downloads,
              bool visual_mode, int visual_start,
              const std::vector<PlaylistItem>& playlist = {}, int playlist_index = -1,
              const std::vector<SavedPlaylistItem>& saved_playlist = {}, int list_selected_idx = 0,
              const std::set<int>& list_marked_indices = {},
              PlayMode play_mode = PlayMode::SINGLE);

    // ---- Input dialogs ----

    // Text input dialog with UTF-8/CJK support and auto-sized window
    std::string input_box(const std::string& prompt, const std::string& default_val = "");

    // Check if input_box result was a user cancellation
    static bool is_input_cancelled(const std::string& result);

    // Simple single-line dialog
    std::string dialog(const std::string& msg);

    // Y/N confirmation dialog with title in border
    bool confirm_box(const std::string& prompt = "Quit?");

    // ---- Help ----

    // Show scrollable help overlay
    void show_help(const MPVController::State& state);

    // ---- Theme ----

    void toggle_theme();
    bool is_dark_mode() const;
    void apply_theme();

private:
    // ---- Windows ----
    WINDOW *left_win_ = nullptr;
    WINDOW *right_win_ = nullptr;
    WINDOW *status_win_ = nullptr;

    // ---- Dimensions ----
    int h_ = 0, w_ = 0;
    int left_w_ = 0, right_w_ = 0, top_h_ = 0;
    int cols_ = 0;  // Track previous column count for resize detection
    float layout_ratio_ = 0.4f;

    // ---- Configuration ----
    StatusBarColorConfig statusbar_color_config_;
    bool show_tree_lines_ = true;
    bool scroll_mode_ = false;
    bool dark_mode_ = true;

    // ---- Layout reference ----
    LayoutMetrics& layout_ = LayoutMetrics::instance();

    // ---- Incremental redraw state tracking ----
    int last_selected_idx_ = -1;
    size_t last_display_size_ = 0;
    int last_view_start_ = -1;
    AppMode last_mode_ = AppMode::RADIO;

    // ---- Private helper methods ----

    // Check if full redraw is needed (currently always returns true)
    bool needs_full_redraw(int selected, size_t list_size, int view_start, AppMode mode);

    // Update redraw tracking state
    void update_redraw_state(int selected, size_t list_size, int view_start, AppMode mode);

    // Draw a single tree node line on the left panel
    void draw_line(WINDOW* win, int y, const DisplayItem& item, bool selected,
                   bool in_visual, int max_len, const std::string& current_url = "");

    // Draw the bottom status bar with version, URL, and author info
    void draw_status(WINDOW* win, const MPVController::State& state, TreeNodePtr selected_node);

    // Draw the right info/log panel
    void draw_info(WINDOW* win, const MPVController::State& state, AppState app_state,
                   TreeNodePtr playback_node, int marked_count, const std::string& search_query,
                   int current_match, int total_matches, TreeNodePtr selected_node,
                   const std::vector<DownloadProgress>& downloads, bool visual_mode, int cw,
                   const std::vector<PlaylistItem>& playlist = {}, int playlist_index = -1,
                   const std::vector<SavedPlaylistItem>& saved_playlist = {},
                   int list_selected_idx = 0);

    // Draw the help overlay window with scroll support
    void draw_help(WINDOW* win, const MPVController::State& state, int cw);
};

} // namespace podradio
