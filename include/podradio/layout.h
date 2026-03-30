#pragma once

#include "common.h"

namespace podradio
{

    // =========================================================
    // LayoutMetrics singleton - unified layout management
    //
    // Manages window size detection, panel metrics calculation,
    // and scroll offset tracking. Automatically resets scroll
    // offsets on terminal resize events.
    // =========================================================
    class LayoutMetrics {
    public:
        // Layout constants
        static constexpr float DEFAULT_LAYOUT_RATIO = 0.4f;
        static constexpr int STATUS_BAR_HEIGHT = 3;
        static constexpr int MIN_CONTENT_WIDTH = 10;
        static constexpr int SAFE_MARGIN = SAFE_BORDER_MARGIN;  // Right border safe buffer columns

        // Window size tracking
        struct WindowSize {
            int lines = 0;
            int cols = 0;
            bool changed = false;  // Whether the size changed since last check
        };

        // Panel metrics
        struct PanelMetrics {
            int left_w = 0;         // Left panel width
            int right_w = 0;        // Right panel width
            int top_h = 0;          // Top height (excluding status bar)
            int content_w = 0;      // Left panel content area width (minus border)
            int safe_content_w = 0; // Safe content area width (minus border and safe buffer)
            int right_inner_w = 0;  // Right panel inner width
            int safe_right_w = 0;   // Safe right panel width
            int status_w = 0;       // Status bar width
            int safe_status_w = 0;  // Safe status bar width
        };

        static LayoutMetrics& instance();

        // Detect window size changes - core entry point for resize handling
        // Returns: true if size changed, full redraw needed
        bool check_resize();

        // Recalculate layout metrics (including safe widths)
        void recalculate_metrics();

        // Reset all scroll offsets - called on resize
        void reset_all_scroll_offsets();

        // Get layout metrics
        const PanelMetrics& get_metrics() const;
        const WindowSize& get_window_size() const;

        // Set layout ratio
        void set_layout_ratio(float ratio);

        // Calculate available net width for a node title row (uses safe width)
        // Parameters: depth - node depth (unused), fixed_width - fixed part width (indent + connector + icon)
        // Returns: available display width for title (with safe buffer reserved)
        int get_title_available_width(int depth, int fixed_width) const;

        // Calculate available net width for right panel log area (uses safe width)
        // Parameters: timestamp_width - width occupied by timestamp
        int get_log_available_width(int timestamp_width = 14) const;

        // Calculate available net width for status bar middle area (uses safe width)
        // Parameters: left_art_w - left art text width, right_art_w - right art text width
        int get_status_available_width(int left_art_w, int right_art_w) const;

        // Scroll offset management interface
        int get_line_scroll_offset(int line_y) const;
        void increment_line_scroll_offset(int line_y);

        int get_log_scroll_offset() const;
        void increment_log_scroll_offset();

        int get_status_scroll_offset() const;
        void increment_status_scroll_offset();

        // Direct access to scroll offset map (backward compatibility)
        std::map<int, int>& get_line_scroll_offsets();

    private:
        LayoutMetrics() {}
        LayoutMetrics(const LayoutMetrics&) = delete;
        LayoutMetrics& operator=(const LayoutMetrics&) = delete;

        WindowSize last_size_;
        PanelMetrics metrics_;
        float layout_ratio_ = DEFAULT_LAYOUT_RATIO;

        // Scroll offset management - unified management of all scroll states
        std::map<int, int> line_scroll_offsets_;  // Left panel per-line scroll offset
        int log_scroll_offset_ = 0;               // Log scroll offset
        int status_scroll_offset_ = 0;            // Status bar URL scroll offset
    };

} // namespace podradio
