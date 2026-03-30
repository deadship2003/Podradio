#pragma once

#include "common.h"

namespace podradio
{

    // =========================================================
    // Utils - static utility class
    //
    // Provides general-purpose helper methods for path expansion,
    // string manipulation, UTF-8 display width calculation,
    // text truncation/wrapping/scrolling, and file operations.
    // =========================================================
    class Utils {
    public:
        // Path expansion
        static std::string expand_path(const std::string& path);
        static std::string get_download_dir();
        static std::string get_log_file();

        // String utilities
        static std::string to_lower(const std::string& s);
        static std::string http_to_https(const std::string& url);
        static std::string execute_command(const std::string& cmd);
        static std::string sanitize_filename(const std::string& name);
        static std::string url_encode(const std::string& s);
        static bool has_gui();

        // UTF-8 character analysis
        static int utf8_char_bytes(unsigned char c);

        // Unicode display width (mk_wcwidth algorithm, Unicode 15.1)
        // Returns: -1 (control char), 0 (non-spacing/combining), 1 (halfwidth), 2 (fullwidth)
        static int mk_wcwidth(uint32_t ucs);

        // UTF-8 display width of a single character (from first byte + continuation bytes)
        static int utf8_char_display_width(unsigned char first_byte, const unsigned char* next_bytes = nullptr);

        // UTF-8 display width of a complete string
        static int utf8_display_width(const std::string& s);

        // Text truncation (left-aligned, with "..." ellipsis)
        static std::string truncate_by_display_width(const std::string& s, int max_display_width);

        // Text truncation from the right (preserves tail)
        static std::string truncate_by_display_width_right(const std::string& s, int max_display_width);

        // Text truncation from the middle (preserves head and tail, "..." in between)
        static std::string truncate_middle(const std::string& s, int max_display_width);

        // Strict truncation - output width never exceeds max_display_width (no ellipsis)
        static std::string truncate_by_display_width_strict(const std::string& s, int max_display_width);

        // Text wrapping - split text into lines at max_width columns
        static std::vector<std::string> wrap_text(const std::string& s, int max_width, int max_lines = 12);

        // Scrolling text for marquee-style display (column-based, not byte-based)
        static std::string get_scrolling_text(const std::string& text, int max_width, int scroll_offset);

        // Formatting utilities
        static std::string format_duration(int seconds);
        static std::string format_size(size_t bytes);

        // File utilities
        static size_t get_file_size(const std::string& path);
        static bool is_partial_download(const std::string& path);
        static std::string find_downloaded_file(const std::string& dir, const std::string& base_name);
    };

} // namespace podradio
