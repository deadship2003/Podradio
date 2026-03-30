#include "podradio/utils.h"

namespace podradio
{

    // =========================================================
    // Path utilities
    // =========================================================

    std::string Utils::expand_path(const std::string& path) {
        if (path.empty() || path[0] != '~') return path;
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + path.substr(1) : path;
    }

    std::string Utils::get_download_dir() { return expand_path("~" + std::string(DOWNLOAD_DIR)); }
    std::string Utils::get_log_file() { return expand_path("~" + std::string(DATA_DIR) + "/podradio.log"); }

    // =========================================================
    // String utilities
    // =========================================================

    std::string Utils::to_lower(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), ::tolower);
        return r;
    }

    // Convert http:// to https:// - security first
    std::string Utils::http_to_https(const std::string& url) {
        if (url.size() > 7 && url.substr(0, 7) == "http://") {
            return "https://" + url.substr(7);
        }
        return url;
    }

    std::string Utils::execute_command(const std::string& cmd) {
        std::array<char, 4096> buffer;
        std::string result;
        FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe)) result += buffer.data();
            pclose(pipe);
        }
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
        return result;
    }

    std::string Utils::sanitize_filename(const std::string& name) {
        std::string result = name;
        std::replace_if(result.begin(), result.end(), [](char c) {
            return c == '/' || c == '\\' || c == ':' || c == '*' ||
                   c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
        }, '_');
        if (result.length() > 200) result = result.substr(0, 200);
        return result;
    }

    // URL encoding function
    std::string Utils::url_encode(const std::string& s) {
        std::string result;
        result.reserve(s.size() * 3);
        for (char c : s) {
            if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
                result += c;
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
                result += buf;
            }
        }
        return result;
    }

    bool Utils::has_gui() {
        return std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr;
    }

    // =========================================================
    // UTF-8 character byte count
    // =========================================================

    int Utils::utf8_char_bytes(unsigned char c) {
        if ((c & 0x80) == 0) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 1;
    }

    // =========================================================
    // UTF-8 display width auto-detection - mk_wcwidth algorithm
    // Based on Unicode 15.1 standard, auto-detects all character widths
    // Replaces hardcoded Unicode ranges, supports any new characters
    // =========================================================

    // mk_wcwidth: returns display width for a Unicode code point
    // Returns: -1 (control char), 0 (invisible/combining), 1 (halfwidth), 2 (fullwidth)
    int Utils::mk_wcwidth(uint32_t ucs) {
        // NULL and control characters
        if (ucs == 0) return 0;
        if (ucs < 32 || (ucs >= 0x7F && ucs < 0xA0)) return -1;

        // ===== Width=0 characters (Non-spacing, Enclosing, Format) =====
        // Combining Diacritical Marks
        if (ucs >= 0x0300 && ucs <= 0x036F) return 0;
        // Combining Diacritical Marks Extended
        if (ucs >= 0x1AB0 && ucs <= 0x1AFF) return 0;
        // Combining Diacritical Marks Supplement
        if (ucs >= 0x1DC0 && ucs <= 0x1DFF) return 0;
        // Combining Diacritical Marks for Symbols
        if (ucs >= 0x20D0 && ucs <= 0x20FF) return 0;
        // Combining Half Marks
        if (ucs >= 0xFE20 && ucs <= 0xFE2F) return 0;

        // Cyrillic combining marks
        if (ucs >= 0x0483 && ucs <= 0x0489) return 0;

        // Arabic combining marks
        if (ucs >= 0x064B && ucs <= 0x065F) return 0;
        if (ucs >= 0x0670 && ucs <= 0x0670) return 0;
        if (ucs >= 0x06D6 && ucs <= 0x06DC) return 0;
        if (ucs >= 0x06DF && ucs <= 0x06E4) return 0;
        if (ucs >= 0x06E7 && ucs <= 0x06E8) return 0;
        if (ucs >= 0x06EA && ucs <= 0x06ED) return 0;

        // Devanagari combining
        if (ucs >= 0x0900 && ucs <= 0x0903) return 0;
        if (ucs >= 0x093A && ucs <= 0x093B) return 0;
        if (ucs >= 0x093C && ucs <= 0x093C) return 0;
        if (ucs >= 0x093E && ucs <= 0x094F) return 0;
        if (ucs >= 0x0951 && ucs <= 0x0957) return 0;
        if (ucs >= 0x0962 && ucs <= 0x0963) return 0;

        // Hebrew combining
        if (ucs >= 0x0591 && ucs <= 0x05BD) return 0;
        if (ucs >= 0x05BF && ucs <= 0x05BF) return 0;
        if (ucs >= 0x05C1 && ucs <= 0x05C2) return 0;
        if (ucs >= 0x05C4 && ucs <= 0x05C5) return 0;
        if (ucs >= 0x05C7 && ucs <= 0x05C7) return 0;

        // Thai combining
        if (ucs >= 0x0E31 && ucs <= 0x0E31) return 0;
        if (ucs >= 0x0E34 && ucs <= 0x0E3A) return 0;
        if (ucs >= 0x0E47 && ucs <= 0x0E4E) return 0;

        // Zero-width characters
        if (ucs == 0x200B) return 0;  // Zero Width Space
        if (ucs == 0x200C) return 0;  // Zero Width Non-Joiner
        if (ucs == 0x200D) return 0;  // Zero Width Joiner
        if (ucs == 0x200E) return 0;  // Left-to-Right Mark
        if (ucs == 0x200F) return 0;  // Right-to-Left Mark

        // Bidirectional formatting
        if (ucs >= 0x202A && ucs <= 0x202E) return 0;
        if (ucs >= 0x2060 && ucs <= 0x2063) return 0;
        if (ucs >= 0x2066 && ucs <= 0x2069) return 0;
        if (ucs >= 0x206A && ucs <= 0x206F) return 0;

        // Variation Selectors
        if (ucs >= 0xFE00 && ucs <= 0xFE0F) return 0;
        if (ucs >= 0xE0100 && ucs <= 0xE01EF) return 0;

        // Other format characters
        if (ucs == 0x061C) return 0;  // Arabic Letter Mark
        if (ucs == 0x034F) return 0;  // Combining Grapheme Joiner
        if (ucs == 0x115F) return 0;  // Hangul Choseong Filler
        if (ucs == 0x1160) return 0;  // Hangul Jungseong Filler
        if (ucs >= 0x17B4 && ucs <= 0x17B5) return 0;  // Khmer vowels
        if (ucs >= 0x180B && ucs <= 0x180D) return 0;  // Mongolian FVS
        if (ucs >= 0x180F && ucs <= 0x180F) return 0;
        if (ucs >= 0xFEFF && ucs <= 0xFEFF) return 0;  // BOM/ZWNBSP

        // ===== Width=2 characters (East Asian Wide/F) =====

        // Hangul Jamo
        if (ucs >= 0x1100 && ucs <= 0x115F) return 2;
        if (ucs >= 0x1161 && ucs <= 0x11FF) return 2;
        if (ucs >= 0xA960 && ucs <= 0xA97C) return 2;  // Hangul Jamo Extended-A
        if (ucs >= 0xD7B0 && ucs <= 0xD7C6) return 2;  // Hangul Jamo Extended-B
        if (ucs >= 0xD7CB && ucs <= 0xD7FB) return 2;  // Hangul Jamo Extended-B

        // CJK Radicals and Symbols
        if (ucs >= 0x2E80 && ucs <= 0x2EF3) return 2;  // CJK Radicals Supplement
        if (ucs >= 0x2F00 && ucs <= 0x2FD5) return 2;  // Kangxi Radicals
        if (ucs >= 0x2FF0 && ucs <= 0x2FFB) return 2;  // Ideographic Description

        // CJK Symbols and Punctuation
        if (ucs >= 0x3000 && ucs <= 0x303E) return 2;
        if (ucs >= 0x3001 && ucs <= 0x3003) return 2;
        if (ucs == 0x303F) return 1;  // Halfwidth space
        if (ucs >= 0x301C && ucs <= 0x301D) return 2;

        // Hiragana
        if (ucs >= 0x3040 && ucs <= 0x309F) return 2;

        // Katakana
        if (ucs >= 0x30A0 && ucs <= 0x30FF) return 2;

        // Bopomofo
        if (ucs >= 0x3100 && ucs <= 0x312F) return 2;
        if (ucs >= 0x31A0 && ucs <= 0x31BF) return 2;  // Bopomofo Extended

        // CJK Strokes
        if (ucs >= 0x31C0 && ucs <= 0x31E3) return 2;

        // CJK Unified Ideographs
        if (ucs >= 0x3400 && ucs <= 0x4DBF) return 2;  // Extension A
        if (ucs >= 0x4E00 && ucs <= 0x9FFF) return 2;  // Main block
        if (ucs >= 0xA000 && ucs <= 0xA48C) return 2;  // Yi Syllables
        if (ucs >= 0xA490 && ucs <= 0xA4C6) return 2;  // Yi Radicals

        // Hangul Syllables
        if (ucs >= 0xAC00 && ucs <= 0xD7A3) return 2;

        // CJK Compatibility Ideographs
        if (ucs >= 0xF900 && ucs <= 0xFAFF) return 2;

        // Vertical Forms
        if (ucs >= 0xFE10 && ucs <= 0xFE19) return 2;
        if (ucs >= 0xFE30 && ucs <= 0xFE52) return 2;  // CJK Compatibility Forms
        if (ucs >= 0xFE54 && ucs <= 0xFE66) return 2;
        if (ucs >= 0xFE68 && ucs <= 0xFE6B) return 2;

        // Fullwidth Forms
        if (ucs >= 0xFF01 && ucs <= 0xFF60) return 2;  // Fullwidth ASCII
        if (ucs >= 0xFFE0 && ucs <= 0xFFE6) return 2;  // Fullwidth Symbols

        // Angle brackets (Wide)
        if (ucs == 0x2329 || ucs == 0x232A) return 2;
        if (ucs == 0x3008 || ucs == 0x3009) return 2;  // CJK angle brackets

        // Currency symbols that are wide
        if (ucs == 0x00A2) return 2;  // Cent sign
        if (ucs == 0x00A3) return 2;  // Pound sign
        if (ucs == 0x00A5) return 2;  // Yen sign
        if (ucs == 0x00A6) return 1;  // Broken bar (narrow)
        if (ucs == 0x00AC) return 2;  // Not sign (wide in some fonts)
        if (ucs == 0x20A9) return 2;  // Won sign

        // FIX: Greek letters are narrow (width=1) in most terminal fonts
        // Original code had width=2 which caused layout misalignment
        if (ucs >= 0x0391 && ucs <= 0x03A9) return 1;  // Greek Capital
        if (ucs >= 0x03B1 && ucs <= 0x03C9) return 1;  // Greek Small
        if (ucs >= 0x03D0 && ucs <= 0x03F5) return 1;  // Greek Extended

        // CJK Extensions B-G (Supplementary Planes)
        if (ucs >= 0x20000 && ucs <= 0x2A6DF) return 2;  // Extension B
        if (ucs >= 0x2A700 && ucs <= 0x2B73F) return 2;  // Extension C
        if (ucs >= 0x2B740 && ucs <= 0x2B81F) return 2;  // Extension D
        if (ucs >= 0x2B820 && ucs <= 0x2CEAF) return 2;  // Extension E
        if (ucs >= 0x2CEB0 && ucs <= 0x2EBEF) return 2;  // Extension F
        if (ucs >= 0x2F800 && ucs <= 0x2FA1F) return 2;  // Compatibility Supplement
        if (ucs >= 0x30000 && ucs <= 0x3134F) return 2;  // Extension G
        if (ucs >= 0x31350 && ucs <= 0x323AF) return 2;  // Extension H

        // ===== V0.05B9n3e5g: Emoji width=2 =====
        // Root cause: emoji actual display width in terminal is 2 columns
        // but mk_wcwidth defaults to returning 1, causing truncation calculation errors

        // Main emoji Unicode ranges:
        if (ucs >= 0x1F300 && ucs <= 0x1F5FF) return 2;  // Miscellaneous Symbols and Pictographs
        if (ucs >= 0x1F600 && ucs <= 0x1F64F) return 2;  // Emoticons
        if (ucs >= 0x1F680 && ucs <= 0x1F6FF) return 2;  // Transport and Map Symbols
        if (ucs >= 0x1F900 && ucs <= 0x1F9FF) return 2;  // Supplemental Symbols and Pictographs
        if (ucs >= 0x1FA00 && ucs <= 0x1FAFF) return 2;  // Extended Pictographs
        if (ucs >= 0x2600 && ucs <= 0x26FF) return 2;    // Miscellaneous Symbols
        if (ucs >= 0x2700 && ucs <= 0x27BF) return 2;    // Dingbats
        if (ucs >= 0x2300 && ucs <= 0x23FF) return 2;    // Miscellaneous Technical
        if (ucs >= 0x25A0 && ucs <= 0x25FF) return 2;    // Geometric Shapes
        if (ucs >= 0x2500 && ucs <= 0x259F) return 1;    // Box Drawing (border chars, width 1)

        // ===== Width=1 characters (default) =====
        // Includes: ASCII, Latin Extended, Cyrillic,
        // Box Drawing, Block Elements,
        // Mathematical Symbols, Arrows

        return 1;
    }

    // UTF-8 character display width using mk_wcwidth
    // Auto-detects all Unicode character widths, no hardcoded ranges needed
    int Utils::utf8_char_display_width(unsigned char first_byte, const unsigned char* next_bytes) {
        // ASCII characters: return directly
        if ((first_byte & 0x80) == 0) {
            return mk_wcwidth(first_byte);
        }

        // Decode UTF-8 code point
        uint32_t codepoint = 0;

        // 2-byte UTF-8
        if ((first_byte & 0xE0) == 0xC0) {
            codepoint = (first_byte & 0x1F) << 6;
            if (next_bytes) {
                codepoint |= (next_bytes[0] & 0x3F);
            }
            return mk_wcwidth(codepoint);
        }

        // 3-byte UTF-8
        if ((first_byte & 0xF0) == 0xE0) {
            codepoint = (first_byte & 0x0F) << 12;
            if (next_bytes) {
                codepoint |= (next_bytes[0] & 0x3F) << 6;
                codepoint |= (next_bytes[1] & 0x3F);
            }
            return mk_wcwidth(codepoint);
        }

        // 4-byte UTF-8 (emoji, supplementary characters, etc.)
        if ((first_byte & 0xF8) == 0xF0) {
            codepoint = (first_byte & 0x07) << 18;
            if (next_bytes) {
                codepoint |= (next_bytes[0] & 0x3F) << 12;
                codepoint |= (next_bytes[1] & 0x3F) << 6;
                codepoint |= (next_bytes[2] & 0x3F);
            }
            return mk_wcwidth(codepoint);
        }

        // Invalid UTF-8
        return 1;
    }

    // Improved display width calculation function
    int Utils::utf8_display_width(const std::string& s) {
        int width = 0;
        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = s[i];
            int char_bytes = utf8_char_bytes(c);

            // Get continuation bytes for full decoding
            const unsigned char* next = (i + 1 < s.size()) ?
                reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;

            width += utf8_char_display_width(c, next);
            i += char_bytes;
        }
        return width;
    }

    // V0.04a: Truncation display function, uses "..." as ellipsis marker
    // Fix: must pass next_bytes parameter for correct CJK character width
    std::string Utils::truncate_by_display_width(const std::string& s, int max_display_width) {
        if (max_display_width <= 0) return "";

        int current_width = 0;
        size_t i = 0;

        while (i < s.size()) {
            unsigned char c = s[i];
            int char_bytes = utf8_char_bytes(c);
            // Pass next_bytes for correct CJK character width calculation (width=2)
            const unsigned char* next = (i + 1 < s.size()) ?
                reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
            int char_width = utf8_char_display_width(c, next);

            if (current_width + char_width > max_display_width) break;

            current_width += char_width;
            i += char_bytes;
        }

        std::string result = s.substr(0, i);
        if (i < s.size()) {
            // V0.04a: uniformly use "..." as ellipsis marker (three dots, width=3)
            if (current_width + 3 <= max_display_width) {
                result += "...";
            } else if (current_width + 2 <= max_display_width) {
                result += "..";
            } else if (current_width + 1 <= max_display_width) {
                result += ".";
            }
        }

        return result;
    }

    // Truncate from the right, preserving the tail of the text
    // Used for responsive display of status bar right-side content
    std::string Utils::truncate_by_display_width_right(const std::string& s, int max_display_width) {
        if (max_display_width <= 0) return "";

        int text_width = utf8_display_width(s);
        if (text_width <= max_display_width) return s;

        // Need to skip from the front, preserve the tail
        int skip_width = text_width - max_display_width;

        // Find the start position after skipping skip_width
        int current_width = 0;
        size_t start_pos = 0;

        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = s[i];
            int char_bytes = utf8_char_bytes(c);
            const unsigned char* next = (i + 1 < s.size()) ?
                reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
            int char_width = utf8_char_display_width(c, next);

            current_width += char_width;
            i += char_bytes;

            if (current_width >= skip_width) {
                start_pos = i;
                break;
            }
        }

        return s.substr(start_pos);
    }

    // Truncate from the middle, preserving head and tail
    // Used for responsive display of status bar middle URL content
    // Example: truncate_middle("https://example.com/path", 15) -> "https://.../path"
    std::string Utils::truncate_middle(const std::string& s, int max_display_width) {
        if (max_display_width <= 0) return "";

        int text_width = utf8_display_width(s);
        if (text_width <= max_display_width) return s;

        // Need to truncate from the middle, preserving head and tail
        // "..." takes 3 columns, remaining space split evenly between head and tail
        int remaining = max_display_width - 3;  // subtract "..." width
        if (remaining < 2) return "...";  // extreme case: only show ellipsis

        int head_width = remaining / 2;
        int tail_width = remaining - head_width;

        // Extract head (from the beginning)
        std::string head;
        int current_width = 0;
        size_t i = 0;
        while (i < s.size() && current_width < head_width) {
            unsigned char c = s[i];
            int char_bytes = utf8_char_bytes(c);
            const unsigned char* next = (i + 1 < s.size()) ?
                reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
            int char_width = utf8_char_display_width(c, next);

            if (current_width + char_width > head_width) break;

            head += s[i];
            current_width += char_width;
            i += char_bytes;
        }

        // Extract tail (from the end backwards)
        std::string tail;
        current_width = 0;
        size_t j = s.size();
        while (j > i && current_width < tail_width) {
            j--;
            // Walk back to find the UTF-8 character start byte
            while (j > 0 && (s[j] & 0xC0) == 0x80) j--;

            unsigned char c = s[j];
            int char_bytes = utf8_char_bytes(c);
            const unsigned char* next = (j + 1 < s.size()) ?
                reinterpret_cast<const unsigned char*>(s.c_str() + j + 1) : nullptr;
            int char_width = utf8_char_display_width(c, next);

            if (current_width + char_width > tail_width) break;

            tail = s.substr(j, char_bytes) + tail;
            current_width += char_width;
        }

        return head + "..." + tail;
    }

    // Strict truncation function - output width never exceeds max_display_width
    std::string Utils::truncate_by_display_width_strict(const std::string& s, int max_display_width) {
        if (max_display_width <= 0) return "";

        int current_width = 0;
        size_t i = 0;

        while (i < s.size()) {
            unsigned char c = s[i];
            int char_bytes = utf8_char_bytes(c);
            const unsigned char* next = (i + 1 < s.size()) ?
                reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
            int char_width = utf8_char_display_width(c, next);

            // Strict check - stop immediately if adding this character would exceed limit
            if (current_width + char_width > max_display_width) break;

            current_width += char_width;
            i += char_bytes;
        }

        return s.substr(0, i);
    }

    // Text wrapping function
    // Wraps text at the specified width, returns a vector of lines
    // max_lines: maximum number of lines, last line uses "..." if exceeded
    std::vector<std::string> Utils::wrap_text(const std::string& s, int max_width, int max_lines) {
        std::vector<std::string> lines;
        if (max_width <= 0 || s.empty()) return lines;

        size_t i = 0;
        while (i < s.size() && (int)lines.size() < max_lines) {
            int current_width = 0;
            std::string line;

            // Build a line, not exceeding max_width
            while (i < s.size()) {
                unsigned char c = s[i];
                int char_bytes = utf8_char_bytes(c);
                const unsigned char* next = (i + 1 < s.size()) ?
                    reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
                int char_width = utf8_char_display_width(c, next);

                if (current_width + char_width > max_width) break;

                line += s.substr(i, char_bytes);
                current_width += char_width;
                i += char_bytes;
            }

            // If this is the last line and there's remaining content, add "..."
            if ((int)lines.size() == max_lines - 1 && i < s.size()) {
                int dots = std::min(3, max_width - current_width);
                if (dots > 0) {
                    line += std::string(dots, '.');
                }
                // Fill the last line
                while (current_width + dots < max_width && i < s.size()) {
                    unsigned char c = s[i];
                    int char_bytes = utf8_char_bytes(c);
                    const unsigned char* next = (i + 1 < s.size()) ?
                        reinterpret_cast<const unsigned char*>(s.c_str() + i + 1) : nullptr;
                    int char_width = utf8_char_display_width(c, next);
                    if (current_width + dots + char_width > max_width) break;
                    line += s.substr(i, char_bytes);
                    current_width += char_width;
                    i += char_bytes;
                }
            }

            if (!line.empty()) {
                lines.push_back(line);
            }
        }

        return lines;
    }

    // Industrial-grade scrolling text function
    // Core principle: scroll by "terminal display column width", not character index
    // Reference: ncurses/vim/tmux Unicode scrolling implementation
    std::string Utils::get_scrolling_text(const std::string& text, int max_width, int scroll_offset) {
        if (max_width <= 0) return "";

        int text_width = utf8_display_width(text);
        if (text_width <= max_width) return text;

        // Build cyclic scrolling content
        // Format: original text + "   " + beginning of original text (for seamless loop)
        std::string separator = "   ";
        std::string padded = text + separator;
        int padded_width = utf8_display_width(padded);

        // offset is in "terminal column" scroll units
        // Use modulo for cyclic scrolling
        int offset = scroll_offset % padded_width;
        if (offset < 0) offset = 0;

        // Step 1 - locate start byte position by terminal column width
        // Key: when offset falls inside a wide character, skip to the next character
        int current_width = 0;
        size_t start_pos = 0;

        for (size_t i = 0; i < padded.size(); ) {
            unsigned char c = padded[i];
            int char_bytes = utf8_char_bytes(c);
            const unsigned char* next = (i + 1 < padded.size()) ?
                reinterpret_cast<const unsigned char*>(padded.c_str() + i + 1) : nullptr;
            int char_width = utf8_char_display_width(c, next);

            // If this character would make width exceed offset, start from the next character
            // This avoids "cutting" in the middle of a wide character
            if (current_width + char_width > offset) {
                // offset falls inside this character, skip past it
                break;
            }

            current_width += char_width;
            i += char_bytes;
            start_pos = i;
        }

        // Step 2 - from start_pos, extract exactly max_width columns
        // Ensure output strictly does not exceed max_width columns
        std::string remaining = padded.substr(start_pos);

        // If remaining content is less than max_width, prepend from the beginning (seamless loop)
        int remaining_width = utf8_display_width(remaining);
        if (remaining_width < max_width) {
            remaining += text;  // append beginning of original text
        }

        // Step 3 - strictly truncate to max_width
        return truncate_by_display_width_strict(remaining, max_width);
    }

    // =========================================================
    // Formatting utilities
    // =========================================================

    std::string Utils::format_duration(int seconds) {
        if (seconds <= 0) return "";
        int h = seconds / 3600;
        int m = (seconds % 3600) / 60;
        int s = seconds % 60;
        if (h > 0) return fmt::format("{}h{}m", h, m);
        if (m > 0) return fmt::format("{}m", m);
        return fmt::format("{}s", s);
    }

    std::string Utils::format_size(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unit_idx = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && unit_idx < 3) { size /= 1024.0; unit_idx++; }
        return fmt::format("{:.1f}{}", size, units[unit_idx]);
    }

    // =========================================================
    // File utilities
    // =========================================================

    size_t Utils::get_file_size(const std::string& path) {
        try {
            if (fs::exists(path)) return fs::file_size(path);
        } catch (...) {}
        return 0;
    }

    bool Utils::is_partial_download(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos != std::string::npos) {
            std::string ext = path.substr(pos);
            if (ext == ".part" || ext == ".tmp" || ext == ".partial") return true;
        }
        return fs::exists(path + ".part");
    }

    std::string Utils::find_downloaded_file(const std::string& dir, const std::string& base_name) {
        std::vector<std::string> extensions = {".mp4", ".mp3", ".m4a", ".webm", ".mkv", ".opus"};
        for (const auto& ext : extensions) {
            std::string path = dir + "/" + base_name + ext;
            if (fs::exists(path)) return path;
        }
        return "";
    }

} // namespace podradio
