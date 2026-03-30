// =========================================================
// UI - Terminal user interface rendering implementation
//
// This is the largest module (~2100 lines), handling all ncurses
// rendering: left panel (tree view), right panel (info/log),
// status bar, help overlay, list popup, input dialogs.
// =========================================================

#include "podradio/ui.h"
#include "podradio/icon_manager.h"
#include "podradio/url_classifier.h"
#include "podradio/config.h"
#include "podradio/event_log.h"
#include "podradio/cache_manager.h"
#include "podradio/sleep_timer.h"
#include "podradio/logger.h"
#include "podradio/utils.h"
#include "podradio/itunes_search.h"
#include "podradio/online_state.h"
#include "podradio/mpv_controller.h"
#include "podradio/statusbar_color.h"
#include "podradio/layout.h"

#include <algorithm>
#include <cstring>
#include <climits>

namespace podradio {

// =========================================================
// File-level helper: draw a complete rectangular border
// Ensures all four corners are properly closed
// =========================================================
void draw_box(WINDOW* win) {
    // Clear window content
    werase(win);

    // Get window dimensions
    int ww, wh;
    getmaxyx(win, wh, ww);

    // Boundary protection: window must be at least 2x2 to draw border
    if (ww < 2 || wh < 2) return;

    // Draw four edges (starting from corners, ensuring closure)
    // Top edge: from top-left corner to top-right corner (excluding corners)
    for (int i = 1; i < ww - 1; i++) {
        mvwaddch(win, 0, i, ACS_HLINE);
    }
    // Bottom edge: from bottom-left corner to bottom-right corner (excluding corners)
    for (int i = 1; i < ww - 1; i++) {
        mvwaddch(win, wh - 1, i, ACS_HLINE);
    }
    // Left edge: from top-left corner to bottom-left corner (excluding corners)
    for (int i = 1; i < wh - 1; i++) {
        mvwaddch(win, i, 0, ACS_VLINE);
    }
    // Right edge: from top-right corner to bottom-right corner (excluding corners)
    for (int i = 1; i < wh - 1; i++) {
        mvwaddch(win, i, ww - 1, ACS_VLINE);
    }

    // Draw four corners (drawn last to ensure closure)
    mvwaddch(win, 0, 0, ACS_ULCORNER);        // Top-left
    mvwaddch(win, 0, ww - 1, ACS_URCORNER);   // Top-right
    mvwaddch(win, wh - 1, 0, ACS_LLCORNER);   // Bottom-left
    mvwaddch(win, wh - 1, ww - 1, ACS_LRCORNER); // Bottom-right
}

// =========================================================
// File-level helper: force-redraw borders to cover overflow
// =========================================================
void protect_border(WINDOW* win, int ww, int wh,
                    int title_start, int title_end,
                    int split_y,
                    int split_title_start, int split_title_end) {

    // Boundary protection: window must be at least 2x2
    if (ww < 2 || wh < 2) return;

    // Step 1: Draw four edges

    // Top edge (skip title embedded area)
    if (title_start < 0) {
        // No embedded title, redraw entire top edge (excluding corners)
        for (int i = 1; i < ww - 1; i++) {
            mvwaddch(win, 0, i, ACS_HLINE);
        }
    } else {
        // Has embedded title, redraw in segments
        for (int i = 1; i < title_start; i++) {
            mvwaddch(win, 0, i, ACS_HLINE);
        }
        for (int i = title_end; i < ww - 1; i++) {
            mvwaddch(win, 0, i, ACS_HLINE);
        }
    }

    // Bottom edge (full redraw, excluding corners)
    for (int i = 1; i < ww - 1; i++) {
        mvwaddch(win, wh - 1, i, ACS_HLINE);
    }

    // Left and right edges (full redraw, excluding corners)
    for (int i = 1; i < wh - 1; i++) {
        mvwaddch(win, i, 0, ACS_VLINE);
        mvwaddch(win, i, ww - 1, ACS_VLINE);
    }

    // Step 2: Draw four corners (drawn last to ensure closure)
    mvwaddch(win, 0, 0, ACS_ULCORNER);        // Top-left
    mvwaddch(win, 0, ww - 1, ACS_URCORNER);   // Top-right
    mvwaddch(win, wh - 1, 0, ACS_LLCORNER);   // Bottom-left
    mvwaddch(win, wh - 1, ww - 1, ACS_LRCORNER); // Bottom-right

    // Step 3: Separator line (if any)
    if (split_y > 0 && split_y < wh - 1) {
        // Draw left and right connectors
        mvwaddch(win, split_y, 0, ACS_LTEE);    // Left tee
        mvwaddch(win, split_y, ww - 1, ACS_RTEE); // Right tee

        // Draw horizontal part of separator
        if (split_title_start < 0) {
            for (int i = 1; i < ww - 1; i++) {
                mvwaddch(win, split_y, i, ACS_HLINE);
            }
        } else {
            for (int i = 1; i < split_title_start; i++) {
                mvwaddch(win, split_y, i, ACS_HLINE);
            }
            for (int i = split_title_end; i < ww - 1; i++) {
                mvwaddch(win, split_y, i, ACS_HLINE);
            }
        }
    }
}

// =========================================================
// UI - Lifecycle methods
// =========================================================

void UI::init(float ratio) {
    // Get terminal size before any terminal operations
    // This is critical for SSH/TTY environments
    int term_rows = 0, term_cols = 0;
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
        term_rows = ws.ws_row;
        term_cols = ws.ws_col;
    }
    // Fallback: get from environment variables
    if (term_rows <= 0 || term_cols <= 0) {
        const char* lines_env = std::getenv("LINES");
        const char* cols_env = std::getenv("COLUMNS");
        if (lines_env) term_rows = std::atoi(lines_env);
        if (cols_env) term_cols = std::atoi(cols_env);
    }
    // If globally saved values are larger, use them
    if (g_original_lines > term_rows) term_rows = g_original_lines;
    if (g_original_cols > term_cols) term_cols = g_original_cols;

    // Set locale (log only, do not print to terminal)
    // Critical fix: do not redirect stdout around setlocale to avoid breaking UTF-8
    char* locale_result = setlocale(LC_ALL, "");
    if (locale_result) {
        LOG(fmt::format("[INIT] Locale set to: {}", locale_result));
    }

    // Initialize ncurses
    initscr();
    g_ncurses_initialized = true;

    // Force set correct terminal size
    if (term_rows > 0 && term_cols > 0) {
        resizeterm(term_rows, term_cols);
        LINES = term_rows;
        COLS = term_cols;
    }
    wrefresh(stdscr);

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(50);  // 50ms to reduce CPU usage (20 FPS)
    start_color();
    use_default_colors();

    for (int i = 0; i < 256; ++i) init_pair(i + 1, i, -1);

    init_pair(10, COLOR_CYAN, COLOR_BLACK);
    init_pair(11, COLOR_GREEN, COLOR_BLACK);
    init_pair(12, COLOR_YELLOW, COLOR_BLACK);
    init_pair(13, COLOR_RED, COLOR_BLACK);
    init_pair(14, COLOR_BLUE, COLOR_BLACK);
    init_pair(15, COLOR_MAGENTA, COLOR_BLACK);

    getmaxyx(stdscr, h_, w_);
    // Use integer arithmetic to avoid floating point precision issues
    layout_ratio_ = ratio;
    left_w_ = w_ * 40 / 100;  // 40% fixed ratio, integer arithmetic
    right_w_ = w_ - left_w_;
    top_h_ = h_ - 3;

    // Boundary protection: ensure minimum window sizes
    if (left_w_ < 10) left_w_ = 10;
    if (right_w_ < 10) right_w_ = 10;
    if (top_h_ < 5) top_h_ = 5;

    left_win_ = newwin(top_h_, left_w_, 0, 0);
    right_win_ = newwin(top_h_, right_w_, 0, left_w_);
    status_win_ = newwin(3, w_, top_h_, 0);

    // Null pointer check to prevent crashes
    if (!left_win_ || !right_win_ || !status_win_) {
        endwin();
        fprintf(stderr, "Error: Failed to create windows\n");
        exit(1);
    }

    // Disable window auto-wrap and scroll to prevent content overflow
    scrollok(left_win_, FALSE);
    scrollok(right_win_, FALSE);
    scrollok(status_win_, FALSE);
    immedok(left_win_, FALSE);
    immedok(right_win_, FALSE);
    immedok(status_win_, FALSE);

    statusbar_color_config_ = IniConfig::instance().get_statusbar_color_config();
    show_tree_lines_ = IniConfig::instance().get_bool("display", "show_tree_lines", true);
    scroll_mode_ = IniConfig::instance().get_bool("display", "scroll_mode", false);  // Default false

    // CacheManager::instance().load() is now called from App::run()
    // after DatabaseManager::init(), so no need to load here.
}

void UI::cleanup() {
    delwin(left_win_);
    delwin(right_win_);
    delwin(status_win_);
    // Use global cleanup function to restore cursor and terminal attributes
    tui_cleanup();
}

// =========================================================
// UI - Toggle settings
// =========================================================

void UI::toggle_tree_lines() {
    show_tree_lines_ = !show_tree_lines_;
    EVENT_LOG(fmt::format("Tree lines: {}", show_tree_lines_ ? "ON" : "OFF"));
}

void UI::toggle_scroll_mode() {
    scroll_mode_ = !scroll_mode_;
    EVENT_LOG(fmt::format("Scroll mode: {}", scroll_mode_ ? "ON" : "OFF"));
}

void UI::set_scroll_mode(bool mode) { scroll_mode_ = mode; }
void UI::set_show_tree_lines(bool show) { show_tree_lines_ = show; }

bool UI::is_scroll_mode() const { return scroll_mode_; }

// =========================================================
// UI - Theme
// =========================================================

void UI::toggle_theme() {
    dark_mode_ = !dark_mode_;
    apply_theme();
    EVENT_LOG(fmt::format("Theme: {}", dark_mode_ ? "Dark" : "Light"));
}

bool UI::is_dark_mode() const { return dark_mode_; }

void UI::apply_theme() {
    if (dark_mode_) {
        // Dark theme: standard ncurses colors
        init_pair(10, COLOR_CYAN, COLOR_BLACK);
        init_pair(11, COLOR_GREEN, COLOR_BLACK);
        init_pair(12, COLOR_YELLOW, COLOR_BLACK);
        init_pair(13, COLOR_RED, COLOR_BLACK);
        init_pair(14, COLOR_BLUE, COLOR_BLACK);
        init_pair(15, COLOR_MAGENTA, COLOR_BLACK);
        // Border colors
        init_pair(20, COLOR_WHITE, COLOR_BLACK);   // Standard border
        init_pair(21, COLOR_CYAN, COLOR_BLACK);    // Info border
    } else {
        // Light theme: inverted color scheme
        init_pair(10, COLOR_CYAN, COLOR_WHITE);
        init_pair(11, COLOR_GREEN, COLOR_WHITE);
        init_pair(12, COLOR_YELLOW, COLOR_WHITE);
        init_pair(13, COLOR_RED, COLOR_WHITE);
        init_pair(14, COLOR_BLUE, COLOR_WHITE);
        init_pair(15, COLOR_MAGENTA, COLOR_WHITE);
        // Border colors
        init_pair(20, COLOR_BLACK, COLOR_WHITE);   // Standard border
        init_pair(21, COLOR_BLUE, COLOR_WHITE);    // Info border
    }
    // Refresh all windows
    werase(left_win_);
    werase(right_win_);
    werase(status_win_);
    box(left_win_, 0, 0);
    box(right_win_, 0, 0);
    box(status_win_, 0, 0);
    wrefresh(left_win_);
    wrefresh(right_win_);
    wrefresh(status_win_);
}

// =========================================================
// UI - Incremental redraw helpers
// =========================================================

bool UI::needs_full_redraw(int /*selected*/, size_t /*list_size*/, int /*view_start*/, AppMode /*mode*/) {
    // Temporarily disable incremental redraw for stability
    return true;
}

void UI::update_redraw_state(int selected, size_t list_size, int view_start, AppMode mode) {
    last_selected_idx_ = selected;
    last_display_size_ = list_size;
    last_view_start_ = view_start;
    last_mode_ = mode;
}

// =========================================================
// UI - draw_line: Render a single tree node line
// =========================================================

void UI::draw_line(WINDOW* win, int y, const DisplayItem& item, bool selected, bool in_visual,
                   int /*max_len*/, const std::string& current_url) {
    // Null pointer check
    if (!item.node) return;

    // Node tree indentation calculation
    // depth=0 (root): no prefix, no connector, has icon
    // depth=1: prefix=" "(1 char) + connector="├─ "(4 chars) = 5 chars
    // depth=2: prefix="  "(2 chars) + connector="├─ "(4 chars) = 6 chars
    // depth=N: prefix=N chars + connector=4 chars

    std::string prefix;
    std::string connector;

    if (show_tree_lines_ && item.depth > 0) {
        // Each parent level uses 1 space indentation
        for (int d = 0; d < item.depth; ++d) {
            prefix += " ";
        }
        // connector="├─ " or "└─ ", occupies 4 character positions
        connector = item.is_last ? "└─ " : "├─ ";
    } else {
        prefix = std::string(item.depth, ' ');
        connector = "";
    }

    std::string icon;
    std::string url = item.node->url.empty() ? "" : item.node->url;

    // Detect currently playing item - highest priority, use green highlight
    bool is_currently_playing = !current_url.empty() && !url.empty() && url == current_url;

    // Distinguish full cache (downloaded) vs partial cache (stream cache)
    bool is_downloaded = !url.empty() &&
                         (CacheManager::instance().is_downloaded(url) || item.node->is_downloaded);
    bool is_stream_cached = !url.empty() && !is_downloaded &&
                            (CacheManager::instance().is_cached(url) || item.node->is_cached);

    // Use IconManager for unified icon management (unified Emoji style)
    if (item.node->marked) icon = IconManager::get_marked();
    else if (item.node->parse_failed) icon = IconManager::get_failed();
    else if (item.node->loading) icon = IconManager::get_loading();
    else if (item.node->type == NodeType::FOLDER || item.node->type == NodeType::PODCAST_FEED)
        icon = item.node->expanded ? IconManager::get_folder_expanded() : IconManager::get_folder_collapsed();
    else if (item.node->type == NodeType::RADIO_STREAM) {
        if (!item.node->children.empty() || !item.node->children_loaded) {
            // Has children or not yet loaded, show folder icon
            icon = item.node->expanded ? IconManager::get_folder_expanded() : IconManager::get_folder_collapsed();
        } else {
            // Leaf radio node, show music icon
            icon = IconManager::get_radio();
        }
    }
    else if (item.node->type == NodeType::PODCAST_EPISODE) {
        if (!url.empty()) {
            URLType url_type = URLClassifier::classify(url);
            if (item.node->is_youtube || url_type == URLType::VIDEO_FILE) icon = IconManager::get_video();
            else icon = IconManager::get_podcast();
        } else {
            icon = IconManager::get_podcast();
        }
    }

    // Icon field fixed width - consistent regardless of ASCII/Emoji
    int icon_width = IconManager::get_icon_field_width();

    // Fixed part (prefix + connector + icon), only title scrolls
    std::string fixed_part = prefix + connector + icon;
    int fixed_width = Utils::utf8_display_width(prefix + connector) + icon_width;

    // Use safe content width to prevent Emoji overflow
    int safe_content_width = layout_.get_metrics().safe_content_w;
    int title_max_len = safe_content_width - fixed_width;
    if (title_max_len < 1) title_max_len = 1;  // At least 1 character

    std::string title_display;
    int title_width = Utils::utf8_display_width(item.node->title);
    if (scroll_mode_ && title_width > title_max_len) {
        // Scroll mode enabled - use industrial scrolling engine
        // Use LayoutMetrics for unified scroll offset management
        layout_.increment_line_scroll_offset(y);
        int scroll_offset = layout_.get_line_scroll_offset(y);
        title_display = Utils::get_scrolling_text(item.node->title, title_max_len, scroll_offset / 5);
    } else {
        // Scroll mode disabled - use strict truncation
        title_display = Utils::truncate_by_display_width_strict(item.node->title, title_max_len);
    }

    // Double protection - ensure final display never exceeds safe content width
    std::string final_display = fixed_part + title_display;
    int final_width = Utils::utf8_display_width(final_display);
    if (final_width > safe_content_width) {
        final_display = Utils::truncate_by_display_width_strict(final_display, safe_content_width);
    }

    // Color priority:
    // 1. Selected/Visual mode - reverse (highest priority)
    // 2. Currently playing - green
    // 3. Full download - magenta
    // 4. Stream cache - cyan
    // 5. Database cache - yellow
    // 6. Parse failed - red
    if (selected || in_visual) wattron(win, A_REVERSE);
    else if (is_currently_playing) wattron(win, COLOR_PAIR(11));    // Green - currently playing
    else if (is_downloaded) wattron(win, COLOR_PAIR(15));           // Magenta - full download
    else if (is_stream_cached) wattron(win, COLOR_PAIR(10));        // Cyan - stream cache
    else if (item.node->is_db_cached) wattron(win, COLOR_PAIR(12)); // Yellow - DB cache
    else if (item.node->parse_failed) wattron(win, COLOR_PAIR(13)); // Red - parse failed

    // Print logic:
    // Window structure: col 0=left border, col 1 to left_w-2=content area, col left_w-1=right border
    // Step 1: Move to row start (col 1, content area start)
    wmove(win, y, 1);

    // Step 2: Clear entire content area row with spaces (prevent residual)
    std::string clear_str(safe_content_width, ' ');
    waddstr(win, clear_str.c_str());

    // Step 3: Move back to row start and print actual content
    wmove(win, y, 1);
    waddstr(win, final_display.c_str());

    if (selected || in_visual) wattroff(win, A_REVERSE);
    else if (is_currently_playing) wattroff(win, COLOR_PAIR(11));
    else if (is_downloaded) wattroff(win, COLOR_PAIR(15));
    else if (is_stream_cached) wattroff(win, COLOR_PAIR(10));
    else if (item.node->is_db_cached) wattroff(win, COLOR_PAIR(12));
    else if (item.node->parse_failed) wattroff(win, COLOR_PAIR(13));
}

// =========================================================
// UI - draw_status: Bottom status bar with smart truncation
// =========================================================
void UI::draw_status(WINDOW* win, const MPVController::State& state, TreeNodePtr selected_node) {
    werase(win);
    box(win, 0, 0);
    int ww = getmaxx(win);
    int inner_w = ww - 2;  // Available width (minus left/right borders)

    // Variable content
    std::string version_str = fmt::format("{} {}", APP_NAME, VERSION);
    std::string author_time = fmt::format("{}@{}", AUTHOR, BUILD_TIME);

    // Get middle URL content (inside brackets)
    std::string mid_content;
    bool show_timer = SleepTimer::instance().is_active();
    if (show_timer) {
        int remaining = SleepTimer::instance().remaining_seconds();
        int hours = remaining / 3600;
        int minutes = (remaining % 3600) / 60;
        int seconds = remaining % 60;
        mid_content = fmt::format("\xe2\x8f\xb0 {:02d}:{:02d}:{:02d}", hours, minutes, seconds);
    } else {
        std::string url_to_show = state.has_media ? state.current_url : (selected_node ? selected_node->url : "");
        url_to_show = Utils::http_to_https(url_to_show);
        mid_content = url_to_show;
    }

    // Calculate widths for each part
    int left_art_w = Utils::utf8_display_width(ART_OUTER_LEFT) + Utils::utf8_display_width(ART_OUTER_RIGHT);
    int right_art_w = Utils::utf8_display_width(ART_RIGHT_PREFIX) + Utils::utf8_display_width(ART_RIGHT_SUFFIX);

    int version_w = Utils::utf8_display_width(version_str);
    int author_w = Utils::utf8_display_width(author_time);
    int mid_w = Utils::utf8_display_width(mid_content);

    int bracket_fixed_w = Utils::utf8_display_width(ART_BRACKET_LEFT) + Utils::utf8_display_width(ART_BRACKET_RIGHT);

    int total_needed = left_art_w + version_w + right_art_w + author_w + bracket_fixed_w + mid_w;

    // Colors
    int left_color = StatusBarColorRenderer::get_color(statusbar_color_config_, 0);
    int right_color = StatusBarColorRenderer::get_color(statusbar_color_config_, 180);
    int mid_color = StatusBarColorRenderer::get_color(statusbar_color_config_, 90);

    // Smart truncation logic
    std::string left_display, mid_display, right_display;

    int fixed_total = left_art_w + right_art_w + bracket_fixed_w;

    if (total_needed <= inner_w) {
        // Case 1: Width sufficient, display everything
        left_display = std::string(ART_OUTER_LEFT) + version_str + ART_OUTER_RIGHT;
        mid_display = mid_content.empty() ? "" : std::string(ART_BRACKET_LEFT) + mid_content + ART_BRACKET_RIGHT;
        right_display = std::string(ART_RIGHT_PREFIX) + author_time + ART_RIGHT_SUFFIX;
    } else if (inner_w < fixed_total + 6) {
        // Case 2: Extremely narrow window, keep art characters, truncate content
        left_display = std::string(ART_OUTER_LEFT) + "..." + ART_OUTER_RIGHT;
        mid_display = std::string(ART_BRACKET_LEFT) + "..." + ART_BRACKET_RIGHT;
        right_display = std::string(ART_RIGHT_PREFIX) + "..." + ART_RIGHT_SUFFIX;
    } else {
        // Case 3: Need priority-based truncation
        int variable_available = inner_w - fixed_total;

        // Phase 1: Try to display version and author fully, truncate middle URL
        int lr_needed = version_w + author_w;
        int mid_available = variable_available - lr_needed;

        if (mid_available >= 3) {
            // Version and author fit, truncate middle URL
            if (mid_w <= mid_available) {
                mid_display = std::string(ART_BRACKET_LEFT) + mid_content + ART_BRACKET_RIGHT;
            } else {
                int inner_bracket_w = mid_available - bracket_fixed_w;
                mid_display = std::string(ART_BRACKET_LEFT) + Utils::truncate_middle(mid_content, inner_bracket_w) + ART_BRACKET_RIGHT;
            }
            left_display = std::string(ART_OUTER_LEFT) + version_str + ART_OUTER_RIGHT;
            right_display = std::string(ART_RIGHT_PREFIX) + author_time + ART_RIGHT_SUFFIX;
        } else {
            // Phase 2: Need to truncate version and/or author
            int mid_min = 3;
            int remaining_for_lr = variable_available - mid_min;

            if (remaining_for_lr < 4) {
                // Space too small, only show art characters
                left_display = ART_OUTER_LEFT;
                mid_display = std::string(ART_BRACKET_LEFT) + "..." + ART_BRACKET_RIGHT;
                right_display = ART_RIGHT_SUFFIX;
            } else {
                int half_remaining = remaining_for_lr / 2;

                // Truncate version
                std::string left_version_part;
                if (version_w <= half_remaining) {
                    left_version_part = version_str;
                } else if (half_remaining > 3) {
                    left_version_part = Utils::truncate_by_display_width(version_str, half_remaining - 3) + "...";
                } else {
                    left_version_part = "...";
                }
                left_display = std::string(ART_OUTER_LEFT) + left_version_part + ART_OUTER_RIGHT;

                // Truncate author time
                int right_remaining = remaining_for_lr - Utils::utf8_display_width(left_version_part);
                std::string right_author_part;
                if (author_w <= right_remaining) {
                    right_author_part = author_time;
                } else if (right_remaining > 3) {
                    right_author_part = "..." + Utils::truncate_by_display_width_right(author_time, right_remaining - 3);
                } else {
                    right_author_part = "...";
                }
                right_display = std::string(ART_RIGHT_PREFIX) + right_author_part + ART_RIGHT_SUFFIX;

                // Middle shows only [...]
                mid_display = std::string(ART_BRACKET_LEFT) + "..." + ART_BRACKET_RIGHT;
            }
        }
    }

    // Center display logic for brackets
    int mid_display_w = Utils::utf8_display_width(mid_display);
    int mid_x = (ww - mid_display_w) / 2;  // Centered position
    if (mid_x < 1) mid_x = 1;  // Protect left border

    // Calculate left maximum width: to middle area left boundary
    int left_max_w = mid_x - 2;
    if (left_max_w < 0) left_max_w = 0;

    // Calculate right maximum width: from middle area right to right border
    int right_start_x = mid_x + mid_display_w + 1;
    int right_max_w = ww - right_start_x - 1;
    if (right_max_w < 0) right_max_w = 0;

    // Check if middle URL needs shrinking
    int left_display_w = Utils::utf8_display_width(left_display);
    int right_display_w = Utils::utf8_display_width(right_display);

    bool need_shrink_mid = false;
    int total_overflow = 0;

    if (left_display_w > left_max_w) {
        total_overflow += (left_display_w - left_max_w);
        need_shrink_mid = true;
    }
    if (right_display_w > right_max_w) {
        total_overflow += (right_display_w - right_max_w);
        need_shrink_mid = true;
    }

    // Shrink middle URL if needed
    if (need_shrink_mid && mid_display_w > 8) {
        int new_mid_w = mid_display_w - total_overflow - 4;
        if (new_mid_w < 8) new_mid_w = 8;

        if (!mid_content.empty()) {
            int inner_bracket_w = new_mid_w - 4;
            if (inner_bracket_w < 3) inner_bracket_w = 3;
            mid_display = "[ " + Utils::truncate_middle(mid_content, inner_bracket_w) + " ]";
        } else {
            mid_display = "[...]";
        }

        // Recalculate centered position
        mid_display_w = Utils::utf8_display_width(mid_display);
        mid_x = (ww - mid_display_w) / 2;
        if (mid_x < 1) mid_x = 1;

        left_max_w = mid_x - 2;
        if (left_max_w < 0) left_max_w = 0;

        right_start_x = mid_x + mid_display_w + 1;
        right_max_w = ww - right_start_x - 1;
        if (right_max_w < 0) right_max_w = 0;
    }

    // Last resort: truncate version if left still overflows
    left_display_w = Utils::utf8_display_width(left_display);
    if (left_display_w > left_max_w && left_max_w > 0) {
        int suffix_w = Utils::utf8_display_width(ART_OUTER_RIGHT);
        int prefix_w = Utils::utf8_display_width(ART_OUTER_LEFT);
        int available_for_version = left_max_w - prefix_w - suffix_w;
        if (available_for_version > 0) {
            left_display = std::string(ART_OUTER_LEFT) +
                Utils::truncate_by_display_width(version_str, available_for_version) +
                ART_OUTER_RIGHT;
        } else {
            left_display = std::string(ART_OUTER_LEFT) + ART_OUTER_RIGHT;
            if (Utils::utf8_display_width(left_display) > left_max_w) {
                left_display = Utils::truncate_by_display_width(left_display, left_max_w);
            }
        }
    }

    // Last resort: truncate author if right still overflows
    right_display_w = Utils::utf8_display_width(right_display);
    if (right_display_w > right_max_w && right_max_w > 0) {
        int suffix_w = Utils::utf8_display_width(ART_RIGHT_SUFFIX);
        int prefix_w = Utils::utf8_display_width(ART_RIGHT_PREFIX);
        int available_for_author = right_max_w - prefix_w - suffix_w;
        if (available_for_author > 0) {
            right_display = std::string(ART_RIGHT_PREFIX) +
                Utils::truncate_by_display_width_right(author_time, available_for_author) +
                ART_RIGHT_SUFFIX;
        } else {
            right_display = std::string(ART_RIGHT_PREFIX) + ART_RIGHT_SUFFIX;
            if (Utils::utf8_display_width(right_display) > right_max_w) {
                right_display = Utils::truncate_by_display_width_right(right_display, right_max_w);
            }
        }
    }

    // Draw status bar (brackets centered)

    // Left side (from x=1, not exceeding middle area left)
    wattron(win, COLOR_PAIR(left_color + 1));
    mvwprintw(win, 1, 1, "%s", left_display.c_str());
    wattroff(win, COLOR_PAIR(left_color + 1));

    // Middle (fixed center)
    if (!mid_display.empty()) {
        wattron(win, COLOR_PAIR(mid_color + 1));
        mvwprintw(win, 1, mid_x, "%s", mid_display.c_str());
        wattroff(win, COLOR_PAIR(mid_color + 1));
    }

    // Right side (right-aligned, not exceeding middle area right)
    right_display_w = Utils::utf8_display_width(right_display);
    int right_x = ww - right_display_w - 1;
    if (right_x < right_start_x) {
        int new_right_w = ww - right_start_x - 1;
        if (new_right_w > 0) {
            int suffix_w = Utils::utf8_display_width(ART_RIGHT_SUFFIX);
            int prefix_w = Utils::utf8_display_width(ART_RIGHT_PREFIX);
            int available_for_author = new_right_w - prefix_w - suffix_w;
            if (available_for_author > 0) {
                right_display = std::string(ART_RIGHT_PREFIX) +
                    Utils::truncate_by_display_width_right(author_time, available_for_author) +
                    ART_RIGHT_SUFFIX;
            } else if (new_right_w >= prefix_w + suffix_w) {
                right_display = std::string(ART_RIGHT_PREFIX) + ART_RIGHT_SUFFIX;
            } else {
                right_display = ART_RIGHT_SUFFIX;
                if (Utils::utf8_display_width(right_display) > new_right_w) {
                    right_display = "";
                }
            }
            right_x = right_start_x;
        } else {
            right_display = "";
            right_x = right_start_x;
        }
    }
    if (!right_display.empty()) {
        wattron(win, COLOR_PAIR(right_color + 1));
        mvwprintw(win, 1, right_x, "%s", right_display.c_str());
        wattroff(win, COLOR_PAIR(right_color + 1));
    }

    // Status bar border protection (no title embedded)
    protect_border(win, ww, 3);
}

// =========================================================
// UI - draw_info: Right panel info and event log
// =========================================================
void UI::draw_info(WINDOW* win, const MPVController::State& state, AppState app_state,
                   TreeNodePtr playback_node, int marked_count, const std::string& search_query,
                   int current_match, int total_matches, TreeNodePtr selected_node,
                   const std::vector<DownloadProgress>& downloads, bool visual_mode, int cw,
                   const std::vector<PlaylistItem>& playlist, int playlist_index,
                   const std::vector<SavedPlaylistItem>& saved_playlist, int list_selected_idx) {
    // Truncation width calculation
    // Right panel structure (window width right_w):
    //   col 0 = left border
    //   col 1 = left margin (space)
    //   col 2 ~ col right_w-3 = content area
    //   col right_w-2 = right margin (space)
    //   col right_w-1 = right border
    int safe_cw = cw;
    if (safe_cw < 1) safe_cw = 1;

    int border_bottom = top_h_ - 1;
    int log_height = std::max(6, static_cast<int>((top_h_ - 2) * 0.4));
    int split_y = border_bottom - log_height;

    int y = 1;

    if (visual_mode) {
        wattron(win, A_BOLD);
        mvwprintw(win, y++, 2, "-- VISUAL MODE --");
        wattroff(win, A_BOLD);
        mvwprintw(win, y++, 2, "j/k: extend | v: confirm | Esc: cancel | V: clear all");
        y++;
    }

    if (!search_query.empty()) {
        wattron(win, A_BOLD);
        std::string search_info = fmt::format("Search: \"{}\" ({}/{})", search_query,
                                              total_matches > 0 ? current_match + 1 : 0, total_matches);
        mvwprintw(win, y++, 2, "%s", Utils::truncate_by_display_width(search_info, safe_cw).c_str());
        wattroff(win, A_BOLD);
        y++;
    }

    // Improved playback state display with volume and speed
    std::string state_str;
    std::string state_icon;
    switch (app_state) {
        case AppState::LOADING: state_icon = "\xe2\x8f\xb3"; state_str = "Loading..."; break;
        case AppState::PLAYING: state_icon = "\xe2\x96\xb6"; state_str = "Playing"; break;
        case AppState::PAUSED: state_icon = "\xe2\x8f\xb8"; state_str = "Paused"; break;
        case AppState::BUFFERING: state_icon = "\xe2\x8f\xb3"; state_str = "Buffering..."; break;
        case AppState::BROWSING: state_icon = "\xf0\x9f\x8e\xaf"; state_str = "Navigating"; break;
        case AppState::LIST_MODE: state_icon = "\xf0\x9f\x93\x8b"; state_str = "List Mode"; break;
        default: state_icon = "\xe2\x97\x8f"; state_str = "Idle"; break;
    }

    // Don't show this status line during play/pause (PLAYER STATUS section shows same info)
    if (app_state != AppState::PLAYING && app_state != AppState::PAUSED) {
        std::string status_line = fmt::format("{} {} | Vol:{}% | Spd:{:.1f}x",
                                               state_icon, state_str, state.volume, state.speed);
        wattron(win, A_BOLD);
        mvwprintw(win, y++, 2, "%s", Utils::truncate_by_display_width(status_line, safe_cw).c_str());
        wattroff(win, A_BOLD);
    }

    if (marked_count > 0) {
        mvwprintw(win, y++, 2, "Marked: %d items", marked_count);
        mvwprintw(win, y++, 2, "Enter: Play | d: Del | D: DL");
        y++;
    }

    // List mode: show selected item details
    if (app_state == AppState::LIST_MODE && !saved_playlist.empty() &&
        list_selected_idx >= 0 && list_selected_idx < static_cast<int>(saved_playlist.size())) {
        y++;
        wattron(win, A_BOLD);
        mvwprintw(win, y++, 2, "--- Selected Item ---");
        wattroff(win, A_BOLD);

        const auto& item = saved_playlist[list_selected_idx];
        mvwprintw(win, y++, 2, "Title: %s",
                  Utils::truncate_by_display_width(item.title, safe_cw - 8).c_str());
        mvwprintw(win, y++, 2, "URL:");
        std::string url_display = "  " + item.url;
        mvwprintw(win, y++, 2, "%s",
                  Utils::truncate_by_display_width(url_display, safe_cw).c_str());

        if (item.duration > 0) {
            int mins = item.duration / 60;
            int secs = item.duration % 60;
            mvwprintw(win, y++, 2, "Duration: %d:%02d", mins, secs);
        }
        mvwprintw(win, y++, 2, "Type: %s", item.is_video ? "Video" : "Audio");
        y++;

        // Show playlist statistics
        mvwprintw(win, y++, 2, "Total items: %zu", saved_playlist.size());
        mvwprintw(win, y++, 2, "Selected: %d/%zu", list_selected_idx + 1, saved_playlist.size());
        y++;
    }

    if (!downloads.empty()) {
        y++;
        wattron(win, A_BOLD);
        mvwprintw(win, y++, 2, "--- Downloads ---");
        wattroff(win, A_BOLD);

        for (const auto& dl : downloads) {
            if (y >= split_y - 3) break;

            // Enhanced download status display
            std::string status_line = dl.title;
            if (dl.active) {
                status_line += fmt::format(" [{}%]", dl.percent);
            } else if (dl.complete) {
                status_line += " [OK]";
            } else if (dl.failed) {
                status_line += " [FAIL]";
            }

            mvwprintw(win, y++, 2, "%s", Utils::truncate_by_display_width(status_line, safe_cw).c_str());

            // ASCII art progress bar + speed + ETA (auto-width)
            if (dl.active && y < split_y - 2 && dl.percent > 0) {
                int bar_width = std::max(MIN_PROGRESS_BAR_WIDTH, safe_cw - PROGRESS_BAR_RESERVED_SPACE);
                if (bar_width > MAX_PROGRESS_BAR_WIDTH) bar_width = MAX_PROGRESS_BAR_WIDTH;
                int filled = (dl.percent * bar_width) / 100;

                // ASCII art progress bar: [████████░░░░░]
                std::string bar = "[";
                for (int i = 0; i < bar_width; ++i) {
                    bar += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91";
                }
                bar += "]";

                // Speed and ETA info (always displayed)
                std::string speed_eta;
                if (!dl.speed.empty()) {
                    speed_eta = dl.speed;
                } else {
                    speed_eta = "...";
                }
                if (dl.eta_seconds > 0) {
                    int eta_mins = dl.eta_seconds / 60;
                    int eta_secs = dl.eta_seconds % 60;
                    speed_eta += fmt::format(" ETA:{}:{:02d}", eta_mins, eta_secs);
                } else if (dl.percent < 100) {
                    speed_eta += " ETA:--:--";
                }

                wattron(win, COLOR_PAIR(11));
                mvwprintw(win, y++, 2, "%s %s", bar.c_str(), speed_eta.c_str());
                wattroff(win, COLOR_PAIR(11));
            }
        }
        y++;
    }

    if (app_state == AppState::PLAYING || app_state == AppState::PAUSED) {
        y++;
        wattron(win, A_BOLD);
        mvwprintw(win, y++, 2, "=== PLAYER STATUS ===");
        wattroff(win, A_BOLD);

        // Show playback state and speed/volume
        std::string play_state = (app_state == AppState::PLAYING) ? "\xe2\x96\xb6 Playing" : "\xe2\x8f\xb8 Paused";
        mvwprintw(win, y++, 2, "%s", play_state.c_str());
        mvwprintw(win, y++, 2, "Speed:  %.1fx", state.speed);
        mvwprintw(win, y++, 2, "Volume: %d%%", state.volume);

        // ASCII art timeline progress bar (auto-width)
        if (state.media_duration > 0) {
            int cur_mins = (int)state.time_pos / 60;
            int cur_secs = (int)state.time_pos % 60;
            int tot_mins = (int)state.media_duration / 60;
            int tot_secs = (int)state.media_duration % 60;

            // Calculate progress percentage
            double progress = state.time_pos / state.media_duration;
            if (progress > 1.0) progress = 1.0;
            if (progress < 0.0) progress = 0.0;

            // Timeline width adapts to window
            int time_str_len = 16;
            int bar_width = cw - time_str_len - 4;
            if (bar_width < MIN_PROGRESS_BAR_WIDTH) bar_width = MIN_PROGRESS_BAR_WIDTH;
            if (bar_width > 40) bar_width = 40;
            int filled = static_cast<int>(progress * bar_width);

            std::string timeline = "[";
            for (int i = 0; i < bar_width; ++i) {
                if (i < filled) {
                    timeline += "\xe2\x96\x93";  // Played portion
                } else if (i == filled) {
                    timeline += (app_state == AppState::PLAYING) ? "\xe2\x96\xb6" : "\xe2\x8f\xb8";  // Playhead
                } else {
                    timeline += "\xe2\x96\x91";  // Unplayed portion
                }
            }
            timeline += "]";

            // Time display
            std::string time_str = fmt::format(" {}:{:02d}/{}:{:02d}", cur_mins, cur_secs, tot_mins, tot_secs);

            wattron(win, A_BOLD);
            mvwprintw(win, y++, 2, "%s%s", timeline.c_str(), time_str.c_str());
            wattroff(win, A_BOLD);
        } else if (state.time_pos > 0) {
            // No total duration, only show current time
            int cur_mins = (int)state.time_pos / 60;
            int cur_secs = (int)state.time_pos % 60;
            mvwprintw(win, y++, 2, "Time:   %d:%02d", cur_mins, cur_secs);
        }

        // Video/Audio info truncation to prevent overflow
        if (!state.audio_codec.empty()) {
            mvwprintw(win, y++, 2, "Audio:  %s",
                      Utils::truncate_by_display_width(state.audio_codec, safe_cw - 9).c_str());
        }
        if (!state.video_codec.empty()) {
            mvwprintw(win, y++, 2, "Video:  %s",
                      Utils::truncate_by_display_width(state.video_codec, safe_cw - 9).c_str());
        }
        y++;

        std::string title_display = (playback_node && !playback_node->title.empty()) ?
                                    playback_node->title : state.title;
        if (!title_display.empty()) {
            mvwprintw(win, y++, 2, "Title: %s",
                      Utils::truncate_by_display_width(title_display, safe_cw - 7).c_str());
        }
    }

    if (selected_node && y < split_y) {
        y++;
        std::string type_str;
        switch (selected_node->type) {
            case NodeType::FOLDER: type_str = "Folder"; break;
            case NodeType::RADIO_STREAM: type_str = "Radio"; break;
            case NodeType::PODCAST_FEED: type_str = "Feed"; break;
            case NodeType::PODCAST_EPISODE: type_str = "Episode"; break;
        }

        URLType url_type = URLClassifier::classify(selected_node->url);

        mvwprintw(win, y++, 2, "Type: %s", type_str.c_str());
        mvwprintw(win, y++, 2, "Title: %s",
                  Utils::truncate_by_display_width(selected_node->title, safe_cw - 7).c_str());

        if (!selected_node->url.empty()) {
            // HTTP to HTTPS display - security first
            std::string url = Utils::http_to_https(selected_node->url);

            // URL display format: label on its own line, URL wrapped with indent
            int url_width = safe_cw - 4;
            if (url_width < 10) url_width = 10;

            // Label on its own line
            mvwprintw(win, y++, 2, "URL:");

            // URL wrapped display, indented 2 spaces
            std::vector<std::string> url_lines = Utils::wrap_text(url, url_width, 12);
            for (size_t li = 0; li < url_lines.size() && y < split_y - 1; ++li) {
                mvwprintw(win, y++, 2, "  %s", url_lines[li].c_str());
            }

            // Show Streaming URL (when different from original URL)
            if (state.has_media && !state.current_url.empty()) {
                std::string streaming_url = Utils::http_to_https(state.current_url);
                if (streaming_url != url) {
                    if (y < split_y - 1) {
                        mvwprintw(win, y++, 2, "Streaming URL:");
                    }
                    int stream_url_width = safe_cw - 4;
                    if (stream_url_width < 10) stream_url_width = 10;

                    std::vector<std::string> stream_lines = Utils::wrap_text(streaming_url, stream_url_width, 12);
                    for (size_t li = 0; li < stream_lines.size() && y < split_y - 1; ++li) {
                        mvwprintw(win, y++, 2, "  %s", stream_lines[li].c_str());
                    }
                }
            }
        }

        // Subtext display - distinguish node types, always truncate
        if (!selected_node->subtext.empty()) {
            std::string label = (selected_node->type == NodeType::PODCAST_FEED) ? "Podcast:" : "Date:";
            int subtext_max_width = safe_cw - 12;
            if (subtext_max_width < 10) subtext_max_width = 10;
            mvwprintw(win, y++, 2, "%s %s", label.c_str(),
                      Utils::truncate_by_display_width(selected_node->subtext, subtext_max_width).c_str());
        }
        if (selected_node->duration > 0) {
            mvwprintw(win, y++, 2, "Dur:   %s", Utils::format_duration(selected_node->duration).c_str());
        }

        if (selected_node->is_cached || CacheManager::instance().is_cached(selected_node->url)) {
            wattron(win, COLOR_PAIR(11));
            mvwprintw(win, y++, 2, " [CACHED]");
            wattroff(win, COLOR_PAIR(11));
        }
        if (selected_node->is_downloaded || CacheManager::instance().is_downloaded(selected_node->url)) {
            wattron(win, COLOR_PAIR(11));
            std::string local = CacheManager::instance().get_local_file(selected_node->url);
            if (!local.empty()) {
                mvwprintw(win, y++, 2, " [DOWNLOADED: %s]",
                          Utils::truncate_by_display_width(local, safe_cw - 16).c_str());
            } else {
                mvwprintw(win, y++, 2, " [DOWNLOADED]");
            }
            wattroff(win, COLOR_PAIR(11));
        }

        if (URLClassifier::is_youtube(url_type)) {
            wattron(win, COLOR_PAIR(12));
            mvwprintw(win, y++, 2, " [YouTube]");
            wattroff(win, COLOR_PAIR(12));
        } else if (url_type == URLType::VIDEO_FILE) {
            wattron(win, COLOR_PAIR(12));
            mvwprintw(win, y++, 2, " [Video]");
            wattroff(win, COLOR_PAIR(12));
        }
    }

    // Playlist display - unified L-mode and default mode
    bool has_saved_playlist = !saved_playlist.empty();
    bool has_playlist = !playlist.empty();

    if ((has_saved_playlist || has_playlist) && y < split_y - 3) {
        y++;
        wattron(win, A_BOLD);
        size_t list_size = has_saved_playlist ? saved_playlist.size() : playlist.size();
        mvwprintw(win, y++, 2, "--- Playlist (%zu items) ---", list_size);
        wattroff(win, A_BOLD);

        // Display 5 items (current + 2 above + 2 below)
        const int max_show = 5;
        int center_idx = has_saved_playlist ? list_selected_idx : playlist_index;

        // Calculate display range centered on current playing item
        int start_idx = center_idx - 2;
        int end_idx = center_idx + 3;

        // Boundary adjustment
        if (start_idx < 0) {
            start_idx = 0;
            end_idx = std::min((int)list_size, max_show);
        }
        if (end_idx > (int)list_size) {
            end_idx = (int)list_size;
            start_idx = std::max(0, end_idx - max_show);
        }

        // Show above truncation notice
        int above_count = start_idx;
        if (above_count > 0) {
            wattron(win, A_DIM);
            mvwprintw(win, y++, 2, "   \xe2\x86\x91 %d more above", above_count);
            wattroff(win, A_DIM);
        }

        for (int i = start_idx; i < end_idx && y < split_y - 2; ++i) {
            bool is_current = (i == playlist_index);

            // Alignment format:
            // Playing icon (2 cols) + 1 space = 3 cols
            // 3 spaces = 3 cols
            std::string prefix;
            if (is_current) {
                prefix = "\xf0\x9f\x94\x8a ";   // Playing icon: emoji(2 cols) + 1 space = 3 cols
            } else {
                prefix = "   ";   // Normal: 3 spaces = 3 cols
            }

            // Format: icon(3 cols) + number + ". " + title
            std::string title_display = prefix + std::to_string(i + 1) + ". " +
                (has_saved_playlist ? saved_playlist[i].title : playlist[i].title);

            if (is_current) {
                wattron(win, COLOR_PAIR(11));  // Green
            }
            mvwprintw(win, y++, 2, "%s",
                      Utils::truncate_by_display_width(title_display, safe_cw - 4).c_str());
            if (is_current) {
                wattroff(win, COLOR_PAIR(11));
            }
        }

        // Show below truncation notice
        int below_count = (int)list_size - end_idx;
        if (below_count > 0 && y < split_y - 2) {
            wattron(win, A_DIM);
            mvwprintw(win, y++, 2, "   \xe2\x86\x93 %d more below", below_count);
            wattroff(win, A_DIM);
        }
        y++;
    }

    // Separator line
    mvwaddch(win, split_y, 0, ACS_LTEE);
    for (int i = 1; i < right_w_ - 1; ++i) mvwaddch(win, split_y, i, ACS_HLINE);
    mvwaddch(win, split_y, right_w_ - 1, ACS_RTEE);
    mvwprintw(win, split_y, 2, " Event Log ");

    // Event log scrolling display
    auto logs = EventLog::instance().get();
    int current_log_y = border_bottom - 1;

    static auto last_scroll_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (scroll_mode_ && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_scroll_time).count() > 200) {
        layout_.increment_log_scroll_offset();
        last_scroll_time = now;
    }

    // Use LayoutMetrics to calculate log area available width
    int timestamp_width = 14;
    int log_msg_width = layout_.get_log_available_width(timestamp_width);

    for (size_t i = 0; i < logs.size() && current_log_y > split_y; ++i) {
        const LogEntry& entry = logs[i];

        std::string msg_display;
        int msg_len = Utils::utf8_display_width(entry.message);

        if (scroll_mode_ && msg_len > log_msg_width) {
            msg_display = Utils::get_scrolling_text(entry.message, log_msg_width, layout_.get_log_scroll_offset() / 5);
        } else {
            msg_display = Utils::truncate_by_display_width(entry.message, log_msg_width);
        }

        mvwprintw(win, current_log_y--, 2, "%s %s",
                  entry.timestamp.c_str(), msg_display.c_str());
    }

    // Right panel border protection
    int right_ww = getmaxx(win);
    int right_wh = getmaxy(win);
    protect_border(win, right_ww, right_wh, 2, 14, split_y, 2, 13);
}

// =========================================================
// UI - draw_help: Scrollable help overlay
// =========================================================
void UI::draw_help(WINDOW* /*win*/, const MPVController::State& /*state*/, int /*cw*/) {
    // Help content definition with all key bindings
    std::vector<std::string> help_lines = {
        fmt::format("{} {}★ - Help", APP_NAME, VERSION),
        "",
        "---- Navigation ----",
        "  k / j      Move up/down",
        "  g / G      Go to first/last item",
        "  PgUp/PgDn  Page up/down",
        "  h          Collapse/Go back",
        "  l / Enter  Expand/Play (marked: add to playlist)",
        "",
        "---- Playback ----",
        "  Space      Play/Pause",
        "  -          Volume down",
        "  [ / ]      Speed slower/faster",
        "  \\          Reset speed to 1.0x",
        "",
        "---- Playlist ----",
        "  + / =      Add to playlist (marked or current)",
        "  L          Open/Close playlist popup",
        "  Ctrl+L     Toggle theme (dark/light)",
        "",
        "---- Actions ----",
        "  a          Add feed (PODCAST) / Subscribe (ONLINE)",
        "  d          Delete node/record (all modes)",
        "  D          Download episode",
        "  B          Batch download / Switch region (ONLINE)",
        "  e          Edit node title/URL",
        "  f          Add to Favourites",
        "  m          Toggle mark",
        "  v          Enter Visual mode",
        "  V          Clear all marks",
        "  C          Clear playlist (keep playing)",
        "  r          Refresh node",
        "  o          Toggle sort order (asc/desc)",
        "",
        "---- Modes ----",
        "  R          Radio mode",
        "  P          Podcast mode",
        "  F          Favourite mode",
        "  H          History mode",
        "  O          Online (iTunes) mode",
        "  M          Cycle through all modes",
        "",
        "---- UI Settings ----",
        "  T          Toggle tree lines",
        "  S          Toggle scroll mode",
        "  U          Toggle icon style (ASCII/Emoji)",
        "",
        "---- Search ----",
        "  /          Search (local) / Search iTunes (ONLINE)",
        "  J / K      Jump to next/prev match",
        "",
        "---- In Playlist Popup (L) ----",
        "  r          Single play mode",
        "  c          Cycle play mode",
        "  y          Random play mode",
        "  J / K      Move item up/down",
        "  d          Delete selected",
        "  C          Clear all",
        "",
        "---- Command Line ----",
        "  -a <url>   Add feed from URL",
        "  -i <file>  Import OPML subscriptions",
        "  -e <file>  Export podcasts to OPML",
        "  -t <time>  Sleep timer (5h/30m/1:25:15)",
        "  --purge    Clear all cached data",
        "",
        "---- Data Storage ----",
        "  Database: ~/.local/share/podradio/podradio.db",
        "  Config:   ~/.config/podradio/config.ini",
        "  Downloads: ~/Downloads/PodRadio/",
        "  Log:      ~/.local/share/podradio/podradio.log",
        "",
        "  Note: All data in SQLite database.",
        "",
        "---- Contact ----",
        "  Email:  Deadship2003@gmail.com",
        "",
        "Press 'q' or '?' to close"
    };

    // Calculate required window size
    int content_height = static_cast<int>(help_lines.size());
    int content_width = 0;
    for (const auto& line : help_lines) {
        int lw = Utils::utf8_display_width(line) + 4;  // Border + margin
        if (lw > content_width) content_width = lw;
    }

    // Limit max size to 90% of screen
    int max_h = static_cast<int>(h_ * 0.9);
    int max_w = static_cast<int>(w_ * 0.9);
    int help_h = std::min(content_height + 2, max_h);
    int help_w = std::min(content_width, max_w);

    // Ensure minimum size
    if (help_h < 10) help_h = 10;
    if (help_w < 40) help_w = 40;

    int help_y = (h_ - help_h) / 2;
    int help_x = (w_ - help_w) / 2;

    WINDOW* help_win = newwin(help_h, help_w, help_y, help_x);
    keypad(help_win, TRUE);

    // Draw border
    box(help_win, 0, 0);

    // Scroll support
    int scroll_offset = 0;
    bool needs_scroll = content_height > help_h - 2;

    auto draw_content = [&]() {
        werase(help_win);
        box(help_win, 0, 0);
        int y = 1;
        int visible_lines = help_h - 2;

        for (int i = scroll_offset; i < content_height && y < help_h - 1; ++i) {
            const std::string& line = help_lines[i];
            std::string display = Utils::truncate_by_display_width(line, help_w - 4);

            if (i == 0) {
                // Title centered
                int title_width = Utils::utf8_display_width(display);
                int x_pos = (help_w - title_width) / 2;
                if (x_pos < 2) x_pos = 2;
                wattron(help_win, A_BOLD);
                mvwprintw(help_win, y++, x_pos, "%s", display.c_str());
                wattroff(help_win, A_BOLD);
            } else if (line.find("----") == 0) {
                // Section header
                wattron(help_win, A_DIM);
                mvwprintw(help_win, y++, 2, "%s", display.c_str());
                wattroff(help_win, A_DIM);
            } else if (line == "Press 'q' or '?' to close") {
                // Bottom hint
                wattron(help_win, A_DIM);
                mvwprintw(help_win, y++, 2, "%s", display.c_str());
                wattroff(help_win, A_DIM);
            } else {
                mvwprintw(help_win, y++, 2, "%s", display.c_str());
            }
        }

        // Scroll indicators
        if (needs_scroll) {
            if (scroll_offset > 0) {
                mvwprintw(help_win, 0, help_w - 6, "\xe2\x96\xb2");
            }
            if (scroll_offset + visible_lines < content_height) {
                mvwprintw(help_win, help_h - 1, help_w - 6, "\xe2\x96\xbc");
            }
        }

        wrefresh(help_win);
    };

    draw_content();

    // Key handling for scrolling
    int ch;
    while ((ch = wgetch(help_win)) != 'q' && ch != '?' && ch != 27) {
        if (ch == 'k' || ch == KEY_UP) {
            if (scroll_offset > 0) {
                scroll_offset--;
                draw_content();
            }
        } else if (ch == 'j' || ch == KEY_DOWN) {
            if (scroll_offset + help_h - 2 < content_height) {
                scroll_offset++;
                draw_content();
            }
        } else if (ch == KEY_PPAGE) {
            scroll_offset = std::max(0, scroll_offset - 5);
            draw_content();
        } else if (ch == KEY_NPAGE) {
            scroll_offset = std::min(content_height - help_h + 2, scroll_offset + 5);
            draw_content();
        } else if (ch == 'g') {
            scroll_offset = 0;
            draw_content();
        } else if (ch == 'G') {
            scroll_offset = std::max(0, content_height - help_h + 2);
            draw_content();
        }
    }

    delwin(help_win);
}

// =========================================================
// UI - input_box: UTF-8/CJK input dialog with auto-sized window
// =========================================================
std::string UI::input_box(const std::string& prompt, const std::string& default_val) {
    // Enable echo and cursor for input method support
    echo();
    curs_set(1);

    // Calculate window width, adaptive to URL length
    int prompt_w = Utils::utf8_display_width(prompt);
    int default_w = default_val.empty() ? 0 : Utils::utf8_display_width(default_val) + 10;
    int min_w = 50;
    int iw = std::max({prompt_w + 20, default_w + 4, min_w});

    // Limit max width to screen width - 4
    int max_w = w_ - 4;
    if (iw > max_w) iw = max_w;

    int ih = 5;  // Default 5 lines
    int default_display_w = default_val.empty() ? 0 : Utils::utf8_display_width(default_val);
    int available_w = iw - 6;
    (void)default_display_w;
    (void)available_w;

    int iy = h_ / 2 - ih / 2, ix = (w_ - iw) / 2;

    WINDOW* inp = newwin(ih, iw, iy, ix);
    keypad(inp, TRUE);
    box(inp, 0, 0);

    // Determine title from prompt
    std::string title;
    std::string lower_prompt = prompt;
    std::transform(lower_prompt.begin(), lower_prompt.end(), lower_prompt.begin(), ::tolower);
    if (lower_prompt.find("search") != std::string::npos) {
        title = " SEARCH ";
    } else if (lower_prompt.find("title") != std::string::npos) {
        title = " EDIT TITLE ";
    } else if (lower_prompt.find("url") != std::string::npos) {
        title = " EDIT URL ";
    } else {
        title = " " + prompt + " ";
    }

    // Line 0: centered title
    int title_display_w = Utils::utf8_display_width(title);
    int title_x = (iw - title_display_w) / 2;
    if (title_x < 1) title_x = 1;
    mvwprintw(inp, 0, title_x, "%s", title.c_str());

    // Line 1: Default value centered (long URL truncation)
    if (!default_val.empty()) {
        wmove(inp, 1, 1);
        for (int i = 1; i < iw - 1; i++) {
            waddch(inp, ' ');
        }

        int prefix_w = 10;  // "Default: " width
        int content_available = iw - 4;

        std::string display_text;
        if (default_display_w <= content_available - prefix_w) {
            display_text = "Default: " + default_val;
        } else {
            int max_url_w = content_available - prefix_w - 3;
            std::string truncated_url = Utils::truncate_by_display_width(default_val, max_url_w);
            display_text = "Default: " + truncated_url + "...";
        }

        int display_w = Utils::utf8_display_width(display_text);
        int display_x = (iw - display_w) / 2;
        if (display_x < 1) display_x = 1;

        if (display_x + display_w > iw - 2) {
            display_x = 2;
        }
        mvwprintw(inp, 1, display_x, "%s", display_text.c_str());
    }

    // Line 3: buttons centered
    std::string btn_enter = "[Enter]Confirm";
    std::string btn_esc = "[ESC]Cancel";
    int left_margin = iw / 6;
    int middle_gap = iw / 3;
    int btn_enter_x = left_margin;
    int btn_esc_x = left_margin + static_cast<int>(btn_enter.length()) + middle_gap;

    if (btn_esc_x + static_cast<int>(btn_esc.length()) > iw - 2) {
        btn_enter_x = 2;
        btn_esc_x = iw - static_cast<int>(btn_esc.length()) - 2;
    }

    mvwprintw(inp, 3, btn_enter_x, "%s", btn_enter.c_str());
    mvwprintw(inp, 3, btn_esc_x, "%s", btn_esc.c_str());

    wrefresh(inp);

    // Input buffer
    std::string input;
    int max_input = iw - 4;

    // Update input display (centered)
    auto update_input_display = [&]() {
        wmove(inp, 2, 1);
        for (int i = 1; i < iw - 1; i++) {
            waddch(inp, ' ');
        }

        if (!input.empty()) {
            int display_width = Utils::utf8_display_width(input);
            int input_x = (iw - display_width) / 2;
            if (input_x < 2) input_x = 2;

            if (input_x + display_width > iw - 2) {
                std::string truncated = Utils::truncate_by_display_width(input, iw - 4);
                display_width = Utils::utf8_display_width(truncated);
                input_x = 2;
                mvwprintw(inp, 2, input_x, "%s", truncated.c_str());
            } else {
                mvwprintw(inp, 2, input_x, "%s", input.c_str());
            }

            wmove(inp, 2, input_x + display_width);
        } else {
            wmove(inp, 2, iw / 2);
        }
        wrefresh(inp);
    };

    update_input_display();

    while (true) {
        int ch = wgetch(inp);

        if (ch == '\n' || ch == KEY_ENTER) {
            break;  // Confirm input
        } else if (ch == 27) {  // ESC key
            // Detect if it's a real ESC cancel
            // Input methods may send ESC sequences, need to check
            nodelay(inp, TRUE);
            int next = wgetch(inp);
            nodelay(inp, FALSE);

            if (next == ERR) {
                // Real ESC key (no following bytes), cancel input
                delwin(inp);
                curs_set(0);
                noecho();
                return INPUT_CANCELLED;
            } else {
                // Part of input method sequence, put back and continue
                ungetch(next);
            }
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            // Backspace - correctly handle UTF-8 multi-byte characters
            if (!input.empty()) {
                size_t pos = input.length() - 1;
                while (pos > 0 && (input[pos] & 0xC0) == 0x80) {
                    pos--;
                }
                input.erase(pos);
                update_input_display();
            }
        } else if (ch == 21) {  // CTRL+U clear all input
            if (!input.empty()) {
                input.clear();
                update_input_display();
            }
        } else if (ch >= 32 && ch < 127) {
            // ASCII character, add directly
            if (input.length() < static_cast<size_t>(max_input - 2)) {
                input += static_cast<char>(ch);
                update_input_display();
            }
        } else if (ch >= 0x80) {
            // UTF-8 character, read complete character
            int expected = 0;
            if ((ch & 0xE0) == 0xC0) expected = 1;
            else if ((ch & 0xF0) == 0xE0) expected = 2;  // Chinese falls here
            else if ((ch & 0xF8) == 0xF0) expected = 3;

            if (input.length() + expected + 1 < static_cast<size_t>(max_input - 2)) {
                input += static_cast<char>(ch);

                // Blocking read for remaining bytes (ensure no loss)
                for (int i = 0; i < expected; i++) {
                    int next = wgetch(inp);
                    if (next != ERR && next > 0) {
                        input += static_cast<char>(next);
                    }
                }
                update_input_display();
            }
        }
    }

    delwin(inp);
    curs_set(0);
    noecho();

    return input.empty() ? default_val : input;
}

bool UI::is_input_cancelled(const std::string& result) {
    return result == INPUT_CANCELLED;
}

// =========================================================
// UI - dialog: Simple single-line dialog
// =========================================================
std::string UI::dialog(const std::string& msg) {
    echo();
    curs_set(1);

    int ih = 3, iw = std::min(static_cast<int>(msg.length()) + 10, w_ - 10);
    int iy = h_ / 2 - 1, ix = (w_ - iw) / 2;

    WINDOW* dlg = newwin(ih, iw, iy, ix);
    box(dlg, 0, 0);
    mvwprintw(dlg, 1, 2, "%s", msg.c_str());
    wrefresh(dlg);

    char buf[16] = {0};
    mvwgetnstr(dlg, 1, static_cast<int>(msg.length()) + 3, buf, sizeof(buf) - 1);

    delwin(dlg);
    noecho();
    curs_set(0);

    return std::string(buf);
}

// =========================================================
// UI - confirm_box: Y/N confirmation dialog
// =========================================================
bool UI::confirm_box(const std::string& prompt) {
    // Extract title from prompt (remove possible "?"/"(CTRL+C)" suffixes)
    std::string title = prompt;
    size_t qpos = title.find('?');
    if (qpos != std::string::npos) {
        title = title.substr(0, qpos);
    }

    // Border title format: " QUIT PODRADIO "
    std::string border_title = " " + title + " ";
    int border_title_w = Utils::utf8_display_width(border_title);

    // Window size: minimum width must fit border title
    int min_w = std::max(border_title_w + 4, 24);
    int ih = 4, iw = min_w;
    int iy = h_ / 2 - ih / 2, ix = (w_ - iw) / 2;

    WINDOW* dlg = newwin(ih, iw, iy, ix);
    keypad(dlg, TRUE);

    // Draw top border with centered title
    waddch(dlg, ACS_ULCORNER);
    int side_dashes = (iw - 2 - border_title_w) / 2;
    for (int i = 0; i < side_dashes; i++) waddch(dlg, ACS_HLINE);
    wprintw(dlg, "%s", border_title.c_str());
    int remaining = iw - 2 - side_dashes - border_title_w;
    for (int i = 0; i < remaining; i++) waddch(dlg, ACS_HLINE);
    waddch(dlg, ACS_URCORNER);

    // Draw side borders and bottom border
    mvwaddch(dlg, 1, 0, ACS_VLINE);
    mvwaddch(dlg, 1, iw - 1, ACS_VLINE);
    mvwaddch(dlg, 2, 0, ACS_VLINE);
    mvwaddch(dlg, 2, iw - 1, ACS_VLINE);
    mvwaddch(dlg, 3, 0, ACS_LLCORNER);
    for (int i = 1; i < iw - 1; i++) waddch(dlg, ACS_HLINE);
    waddch(dlg, ACS_LRCORNER);

    // YES/NO distributed at bottom sides
    mvwprintw(dlg, 2, 2, "[Y]es");
    mvwprintw(dlg, 2, iw - 7, "[N]o");

    wrefresh(dlg);

    // Wait for user input
    int ch;
    bool result = false;
    while ((ch = wgetch(dlg)) != ERR) {
        if (ch == 'y' || ch == 'Y') {
            result = true;
            break;
        } else if (ch == 'n' || ch == 'N' || ch == 27) {  // ESC also cancels
            result = false;
            break;
        }
    }

    delwin(dlg);
    return result;
}

// =========================================================
// UI - show_help: Public wrapper for help overlay
// =========================================================
void UI::show_help(const MPVController::State& state) {
    draw_help(nullptr, state, 0);
}

// =========================================================
// UI - draw: Main render loop
// =========================================================
void UI::draw(AppMode mode, const std::vector<DisplayItem>& list, int selected,
              const MPVController::State& state, int view_start,
              AppState app_state, TreeNodePtr playback_node, int marked_count,
              const std::string& search_query, int current_match, int total_matches,
              TreeNodePtr selected_node, const std::vector<DownloadProgress>& downloads,
              bool visual_mode, int visual_start,
              const std::vector<PlaylistItem>& playlist, int playlist_index,
              const std::vector<SavedPlaylistItem>& saved_playlist, int list_selected_idx,
              const std::set<int>& list_marked_indices,
              PlayMode play_mode) {

    getmaxyx(stdscr, h_, w_);
    // Use integer arithmetic to avoid floating point precision issues
    int new_left_w = w_ * 40 / 100;
    int new_right_w = w_ - new_left_w;
    int new_top_h = h_ - 3;

    // Detect window size changes
    bool size_changed = (new_left_w != left_w_ || new_right_w != right_w_ || new_top_h != top_h_ || w_ != cols_);

    if (size_changed) {
        // Clear all windows when size changes
        werase(left_win_);
        werase(right_win_);
        werase(status_win_);
        werase(stdscr);

        // Update cached dimensions
        left_w_ = new_left_w;
        right_w_ = new_right_w;
        top_h_ = new_top_h;
        cols_ = w_;
    }

    wresize(left_win_, top_h_, left_w_);
    wresize(right_win_, top_h_, right_w_);
    mvwin(right_win_, 0, left_w_);
    wresize(status_win_, 3, w_);
    mvwin(status_win_, top_h_, 0);

    // Conservative title max width calculation, protect right-top corner
    int title_max = left_w_ - 6;
    if (title_max < 5) title_max = 5;

    std::string mode_str;
    switch (mode) {
        case AppMode::RADIO: mode_str = "\xf0\x9f\x93\xbb RADIO"; break;
        case AppMode::PODCAST: mode_str = "\xf0\x9f\x8e\x99 PODCAST"; break;
        case AppMode::FAVOURITE: mode_str = "\xf0\x9f\x92\x96 FAVOURITE"; break;
        case AppMode::HISTORY: mode_str = "\xf0\x9f\x93\x9c HISTORY"; break;
        case AppMode::ONLINE: mode_str = fmt::format("\xf0\x9f\x94\x8d ONLINE [{}]",
            ITunesSearch::get_region_name(OnlineState::instance().current_region)); break;
    }
    if (visual_mode) mode_str = "\xc2\xa7 VISUAL \xc2\xa7";
    if (marked_count > 0) mode_str += " [\xf0\x9f\x94\x96 x" + std::to_string(marked_count) + "]";
    if (show_tree_lines_) mode_str += " [T]";
    if (scroll_mode_) mode_str += " [S]";

    std::string title_display = Utils::truncate_by_display_width(mode_str, title_max);

    // Tree line width
    int tree_line_width = left_w_ - 2;
    if (tree_line_width < 10) tree_line_width = 10;

    // ═══════════════════════════════════════════════════════
    // List mode: centered popup display
    // ═══════════════════════════════════════════════════════
    if (app_state == AppState::LIST_MODE) {
        // Draw normal main interface (three rectangular borders)
        draw_box(left_win_);
        mvwprintw(left_win_, 0, 2, " %s ", title_display.c_str());
        mvwaddch(left_win_, 0, left_w_ - 1, ACS_URCORNER);
        mvwaddch(left_win_, 0, left_w_ - 2, ACS_HLINE);

        int line_num = 1;
        for (int i = view_start; i < static_cast<int>(list.size()) && line_num < top_h_ - 1; ++i) {
            bool is_sel = (i == selected);
            bool in_visual = visual_mode && visual_start >= 0 &&
                ((visual_start <= i && i <= selected) || (selected <= i && i <= visual_start));
            draw_line(left_win_, line_num, list[i], is_sel, in_visual, tree_line_width, state.current_url);
            line_num++;
        }

        // Ensure right panel and status bar borders are fully drawn
        draw_box(right_win_);
        mvwprintw(right_win_, 0, 2, " INFO & LOG ");
        draw_info(right_win_, state, AppState::BROWSING, playback_node, marked_count, search_query,
                  current_match, total_matches, selected_node, downloads, visual_mode, right_w_ - 3,
                  playlist, playlist_index, saved_playlist, list_selected_idx);
        draw_box(status_win_);
        draw_status(status_win_, state, selected_node);
        wnoutrefresh(left_win_);
        wnoutrefresh(right_win_);
        wnoutrefresh(status_win_);

        // Draw centered popup (box mode)
        int popup_h = std::min(28, h_ * 7 / 10);
        int popup_w = std::max(70, w_ * 8 / 10);
        int popup_y = (h_ - popup_h) / 2;
        int popup_x = (w_ - popup_w) / 2;

        WINDOW* popup = newwin(popup_h, popup_w, popup_y, popup_x);
        keypad(popup, TRUE);

        box(popup, 0, 0);

        // Get play mode icon
        std::string mode_icon;
        std::string mode_name;
        switch (play_mode) {
            case PlayMode::SINGLE: mode_icon = "\xf0\x9f\x94\x82"; mode_name = "Single"; break;
            case PlayMode::CYCLE: mode_icon = "\xf0\x9f\x94\x81"; mode_name = "Cycle"; break;
            case PlayMode::RANDOM: mode_icon = "\xf0\x9f\x94\x80"; mode_name = "Random"; break;
        }

        std::string popup_title = fmt::format("\xf0\x9f\x93\x8b PLAYLIST ({}) {}{}",
            saved_playlist.size(), mode_icon, mode_name);
        int title_w = Utils::utf8_display_width(popup_title);
        int title_x = (popup_w - title_w) / 2;
        if (title_x < 1) title_x = 1;

        wattron(popup, A_BOLD);
        mvwprintw(popup, 0, title_x, "%s", popup_title.c_str());
        wattroff(popup, A_BOLD);

        // Visible lines calculation
        int visible_lines = popup_h - 5;
        if (visible_lines < 1) visible_lines = 1;

        // Calculate scroll offset
        int scroll_off = 0;
        if (list_selected_idx >= visible_lines - 1) {
            scroll_off = list_selected_idx - visible_lines + 2;
        }

        int above_count = scroll_off;
        int below_count = std::max(0, static_cast<int>(saved_playlist.size()) - scroll_off - visible_lines);

        // Right-top corner scroll indicator
        if (above_count > 0) {
            mvwprintw(popup, 0, popup_w - 3, "\xe2\x96\xb2");
        }

        // Draw content area
        int content_y = 1;
        int content_w = popup_w - 2;

        // Currently playing URL
        std::string playing_url = state.has_media ? state.current_url : "";

        for (size_t i = static_cast<size_t>(scroll_off); i < saved_playlist.size() && content_y < popup_h - 4; ++i) {
            const auto& item = saved_playlist[i];
            bool is_selected = (static_cast<int>(i) == list_selected_idx);
            bool is_playing = (item.url == playing_url);

            // Check if invalid
            bool is_invalid = item.url.empty();

            // Build number part (fixed 4-character width)
            std::string num_part = std::to_string(i + 1) + ". ";
            while (num_part.length() < 4) num_part += " ";
            if (num_part.length() > 4) num_part = num_part.substr(0, 4);

            // Status mark (on right side)
            std::string status_mark;
            if (is_invalid) {
                status_mark = " \xe2\x9a\xa0";  // Warning
            } else if (is_playing) {
                status_mark = " \xf0\x9f\x94\x8a";  // Speaker
            }

            // Mark icon (before number)
            std::string mark_prefix;
            if (list_marked_indices.count(static_cast<int>(i))) {
                mark_prefix = "\xf0\x9f\x94\x96 ";
                num_part = "";  // Remove number when marked, keep alignment
            }

            // Calculate title available width
            int title_max_w = content_w - 4 - Utils::utf8_display_width(status_mark) - Utils::utf8_display_width(mark_prefix);
            if (title_max_w < 10) title_max_w = 10;

            std::string truncated_title = Utils::truncate_by_display_width(item.title, title_max_w);

            // Build complete line
            std::string line = mark_prefix + num_part + truncated_title + status_mark;

            // Highlight
            if (is_selected) {
                wattron(popup, A_REVERSE);
            } else if (is_playing) {
                wattron(popup, COLOR_PAIR(11));  // Green highlight
            } else if (is_invalid) {
                wattron(popup, COLOR_PAIR(13) | A_DIM);  // Red dim
            }

            std::string truncated = Utils::truncate_by_display_width(line, content_w);
            mvwprintw(popup, content_y, 1, "%s", truncated.c_str());

            // Clear remaining part of the line
            int line_w = Utils::utf8_display_width(truncated);
            for (int j = line_w + 1; j < content_w; ++j) {
                waddch(popup, ' ');
            }

            if (is_selected) {
                wattroff(popup, A_REVERSE);
            } else if (is_playing) {
                wattroff(popup, COLOR_PAIR(11));
            } else if (is_invalid) {
                wattroff(popup, COLOR_PAIR(13) | A_DIM);
            }

            content_y++;
        }

        // Fill blank lines
        while (content_y < popup_h - 4) {
            for (int j = 1; j < popup_w - 1; ++j) {
                mvwaddch(popup, content_y, j, ' ');
            }
            content_y++;
        }

        // Draw separator line
        mvwaddch(popup, popup_h - 4, 0, ACS_LTEE);
        for (int i = 1; i < popup_w - 1; ++i) waddch(popup, ACS_HLINE);
        mvwaddch(popup, popup_h - 4, popup_w - 1, ACS_RTEE);

        // First hotkey line (centered) - action keys
        std::string help_line1 = "[L]Close [j/k]Nav [g/G]Top/End [d]Del [J/K]Move [l/Enter]Play [+]Add";
        int help1_w = Utils::utf8_display_width(help_line1);
        int help1_x = (popup_w - help1_w) / 2;
        if (help1_x < 1) help1_x = 1;
        mvwprintw(popup, popup_h - 3, help1_x, "%s", help_line1.c_str());

        // Second hotkey line (centered) - mode keys
        std::string help_line2 = "[r]Single [c]Cycle [y]Random [C]ClearAll";
        int help2_w = Utils::utf8_display_width(help_line2);
        int help2_x = (popup_w - help2_w) / 2;
        if (help2_x < 1) help2_x = 1;
        mvwprintw(popup, popup_h - 2, help2_x, "%s", help_line2.c_str());

        // Bottom-right corner scroll indicator
        if (below_count > 0) {
            mvwprintw(popup, popup_h - 2, popup_w - 3, "\xe2\x96\xbc");
        }

        wnoutrefresh(popup);
        doupdate();

        delwin(popup);

        return;  // List mode rendering complete
    }

    // Incremental redraw optimization (simplified: check boundary conditions)
    bool full_redraw = needs_full_redraw(selected, list.size(), view_start, mode);

    // Boundary protection: force full redraw when list empty or selection invalid
    if (list.empty() || selected < 0 || selected >= static_cast<int>(list.size())) {
        full_redraw = true;
    }

    // Window size changed: force full redraw
    if (size_changed) {
        full_redraw = true;
    }

    int line_num = 1;

    if (full_redraw) {
        // Full redraw: clear window and draw all lines
        draw_box(left_win_);
        mvwprintw(left_win_, 0, 2, " %s ", title_display.c_str());
        mvwaddch(left_win_, 0, left_w_ - 1, ACS_URCORNER);
        mvwaddch(left_win_, 0, left_w_ - 2, ACS_HLINE);

        for (int i = view_start; i < static_cast<int>(list.size()) && line_num < top_h_ - 1; ++i) {
            bool is_sel = (i == selected);
            bool in_visual = visual_mode && visual_start >= 0 &&
                ((visual_start <= i && i <= selected) || (selected <= i && i <= visual_start));
            draw_line(left_win_, line_num, list[i], is_sel, in_visual, tree_line_width, state.current_url);
            line_num++;
        }
    } else {
        // Incremental redraw: only redraw cursor-changed lines
        int old_screen_row = last_selected_idx_ - view_start + 1;
        int new_screen_row = selected - view_start + 1;

        // Redraw old line (remove highlight) with full boundary check
        if (old_screen_row >= 1 && old_screen_row < top_h_ - 1 &&
            last_selected_idx_ >= 0 && last_selected_idx_ < static_cast<int>(list.size())) {
            bool in_visual_old = visual_mode && visual_start >= 0 &&
                ((visual_start <= last_selected_idx_ && last_selected_idx_ <= selected) ||
                 (selected <= last_selected_idx_ && last_selected_idx_ <= visual_start));
            draw_line(left_win_, old_screen_row, list[last_selected_idx_], false, in_visual_old, tree_line_width, state.current_url);
        }

        // Redraw new line (highlight) with full boundary check
        if (new_screen_row >= 1 && new_screen_row < top_h_ - 1 &&
            selected >= 0 && selected < static_cast<int>(list.size())) {
            bool in_visual_new = visual_mode && visual_start >= 0 &&
                ((visual_start <= selected && selected <= visual_start) ||
                 (visual_start <= selected));
            draw_line(left_win_, new_screen_row, list[selected], true, in_visual_new, tree_line_width, state.current_url);
        }
    }

    // Update tracking state
    update_redraw_state(selected, list.size(), view_start, mode);

    // Left panel border protection
    int left_title_end = 2 + Utils::utf8_display_width(title_display) + 2;
    protect_border(left_win_, left_w_, top_h_, 2, left_title_end);
    wnoutrefresh(left_win_);

    // Right Panel
    draw_box(right_win_);
    mvwprintw(right_win_, 0, 2, " INFO & LOG ");

    // Use getmaxx for actual window width
    int right_inner_width = getmaxx(right_win_) - 3;

    draw_info(right_win_, state, app_state, playback_node, marked_count,
              search_query, current_match, total_matches, selected_node, downloads,
              visual_mode, right_inner_width, playlist, playlist_index,
              saved_playlist, list_selected_idx);

    // Right panel border protection
    protect_border(right_win_, right_w_, top_h_, 2, 13);
    wnoutrefresh(right_win_);

    // Status Bar
    draw_status(status_win_, state, selected_node);
    wnoutrefresh(status_win_);

    // Double-buffered refresh - update all windows at once
    doupdate();
}

} // namespace podradio
