#include "podradio/layout.h"

namespace podradio
{

    LayoutMetrics& LayoutMetrics::instance() {
        static LayoutMetrics lm;
        return lm;
    }

    // Detect window size changes - core entry point
    // Returns: true if size changed, full redraw needed
    bool LayoutMetrics::check_resize() {
        int current_lines = LINES;
        int current_cols = COLS;

        last_size_.changed = (current_lines != last_size_.lines ||
                              current_cols != last_size_.cols);

        if (last_size_.changed) {
            last_size_.lines = current_lines;
            last_size_.cols = current_cols;

            // Key - reset all scroll offsets on resize
            // because available net width changed, old scroll positions may be invalid
            reset_all_scroll_offsets();

            // Recalculate layout
            recalculate_metrics();
        }

        return last_size_.changed;
    }

    // Recalculate layout metrics (including safe widths)
    void LayoutMetrics::recalculate_metrics() {
        metrics_.status_w = last_size_.cols;
        metrics_.top_h = last_size_.lines - STATUS_BAR_HEIGHT;
        // Use integer arithmetic to avoid floating point precision issues
        metrics_.left_w = last_size_.cols * 40 / 100;
        metrics_.right_w = last_size_.cols - metrics_.left_w;

        // Content area width = window width - left and right borders (2)
        metrics_.content_w = metrics_.left_w - 2;
        if (metrics_.content_w < MIN_CONTENT_WIDTH) {
            metrics_.content_w = MIN_CONTENT_WIDTH;
        }

        // Safe content area width = content area width - safe buffer (1)
        // Prevents emoji width inconsistency from breaking borders
        metrics_.safe_content_w = metrics_.content_w - SAFE_MARGIN;
        if (metrics_.safe_content_w < MIN_CONTENT_WIDTH) {
            metrics_.safe_content_w = MIN_CONTENT_WIDTH;
        }

        // Fix right panel inner width calculation
        // Content area width = right_w - 2 (left and right borders, 1 column each)
        // Print starts from column 2, available width = right_w - 3
        metrics_.right_inner_w = metrics_.right_w - 3;
        if (metrics_.right_inner_w < MIN_CONTENT_WIDTH) {
            metrics_.right_inner_w = MIN_CONTENT_WIDTH;
        }

        // Safe right panel width
        metrics_.safe_right_w = metrics_.right_inner_w - SAFE_MARGIN;
        if (metrics_.safe_right_w < MIN_CONTENT_WIDTH) {
            metrics_.safe_right_w = MIN_CONTENT_WIDTH;
        }

        // Safe status bar width
        metrics_.safe_status_w = metrics_.status_w - SAFE_MARGIN;
    }

    // Reset all scroll offsets - called on resize
    void LayoutMetrics::reset_all_scroll_offsets() {
        line_scroll_offsets_.clear();
        log_scroll_offset_ = 0;
        status_scroll_offset_ = 0;
    }

    // Get layout metrics
    const LayoutMetrics::PanelMetrics& LayoutMetrics::get_metrics() const { return metrics_; }
    const LayoutMetrics::WindowSize& LayoutMetrics::get_window_size() const { return last_size_; }

    // Set layout ratio
    void LayoutMetrics::set_layout_ratio(float ratio) {
        if (ratio > 0.2f && ratio < 0.8f) {
            layout_ratio_ = ratio;
            recalculate_metrics();
        }
    }

    // Calculate available net width for a node title row (uses safe width)
    int LayoutMetrics::get_title_available_width(int /*depth*/, int fixed_width) const {
        // Use safe content area width to prevent emoji width overflow
        int available = metrics_.safe_content_w - fixed_width;
        return (available > 0) ? available : 1;
    }

    // Calculate available net width for right panel log area (uses safe width)
    int LayoutMetrics::get_log_available_width(int timestamp_width) const {
        // Use safe right panel width
        int available = metrics_.safe_right_w - timestamp_width - 1;
        return (available > 0) ? available : 1;
    }

    // Calculate available net width for status bar middle area (uses safe width)
    int LayoutMetrics::get_status_available_width(int left_art_w, int right_art_w) const {
        // Use safe status bar width
        int available = metrics_.safe_status_w - left_art_w - right_art_w - 4;
        return (available > 10) ? available : 0;
    }

    // Scroll offset management
    int LayoutMetrics::get_line_scroll_offset(int line_y) const {
        auto it = line_scroll_offsets_.find(line_y);
        return (it != line_scroll_offsets_.end()) ? it->second : 0;
    }

    void LayoutMetrics::increment_line_scroll_offset(int line_y) {
        line_scroll_offsets_[line_y]++;
    }

    int LayoutMetrics::get_log_scroll_offset() const { return log_scroll_offset_; }
    void LayoutMetrics::increment_log_scroll_offset() { log_scroll_offset_++; }

    int LayoutMetrics::get_status_scroll_offset() const { return status_scroll_offset_; }
    void LayoutMetrics::increment_status_scroll_offset() { status_scroll_offset_++; }

    // Direct access to scroll offset map (backward compatibility)
    std::map<int, int>& LayoutMetrics::get_line_scroll_offsets() { return line_scroll_offsets_; }

} // namespace podradio
