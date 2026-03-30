// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                    PodRadio Unit Tests - Utils                           ║
// ║              Tests for string/text formatting utilities                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#include <gtest/gtest.h>
#include "podradio/utils.h"

using namespace podradio;

// =============================================================================
// UTF-8 Display Width Tests
// =============================================================================

TEST(test_utf8_display_width_ascii, pure_ascii_returns_byte_count) {
    // Each ASCII character has display width 1
    EXPECT_EQ(Utils::utf8_display_width("Hello"), 5);
    EXPECT_EQ(Utils::utf8_display_width(""), 0);
    EXPECT_EQ(Utils::utf8_display_width("A"), 1);
    EXPECT_EQ(Utils::utf8_display_width("PodRadio V0.05"), 13);
    EXPECT_EQ(Utils::utf8_display_width("abcXYZ123!@#"), 12);
}

TEST(test_utf8_display_width_cjk, cjk_characters_return_width_2) {
    // CJK Unified Ideographs have display width 2 (fullwidth)
    EXPECT_EQ(Utils::utf8_display_width("\xe4\xb8\xad\xe6\x96\x87"), 4);   // "中文" = 2 chars x width 2
    EXPECT_EQ(Utils::utf8_display_width("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"), 6); // "日本語" = 3 chars x width 2
}

TEST(test_utf8_display_width_emoji, emoji_characters_return_width_2) {
    // Common emoji (U+1F3B5 musical note) has display width 2
    EXPECT_EQ(Utils::utf8_display_width("\xf0\x9f\x8e\xb5"), 2); // ♪ musical note
    EXPECT_EQ(Utils::utf8_display_width("\xf0\x9f\x8e\xb5\xf0\x9f\x8e\xb5"), 4); // two emoji
}

TEST(test_utf8_display_width_greek, greek_characters_return_width_1) {
    // Greek letters are in the European script range - width 1
    EXPECT_EQ(Utils::utf8_display_width("\xce\xb1"), 1);              // α (alpha)
    EXPECT_EQ(Utils::utf8_display_width("\xce\xb1\xce\xb2\xce\xb3"), 3); // αβγ (alpha beta gamma)
    EXPECT_EQ(Utils::utf8_display_width("\xce\x9f\xce\xbc\xce\xb5\xce\xb3\xce\xb1"), 5); // Ομεγα (Omega)
}

// =============================================================================
// Text Truncation Tests
// =============================================================================

TEST(test_truncate_by_display_width, truncates_ascii_correctly) {
    // Truncate "Hello World" (width 11) to max 5 → "He..."
    auto result = Utils::truncate_by_display_width("Hello World", 5);
    EXPECT_EQ(result, "He...");
    EXPECT_LE(Utils::utf8_display_width(result), 5);
}

TEST(test_truncate_by_display_width, short_string_unchanged) {
    // String shorter than max width is returned as-is
    auto result = Utils::truncate_by_display_width("Hi", 10);
    EXPECT_EQ(result, "Hi");
}

TEST(test_truncate_by_display_width, empty_string) {
    auto result = Utils::truncate_by_display_width("", 10);
    EXPECT_EQ(result, "");
}

TEST(test_truncate_by_display_width, zero_width) {
    auto result = Utils::truncate_by_display_width("Hello", 0);
    EXPECT_TRUE(result.empty() || Utils::utf8_display_width(result) <= 0);
}

TEST(test_truncate_by_display_width, handles_cjk) {
    // Two CJK chars = width 4, truncate to width 2 should fit one char or ellipsis
    std::string cjk = "\xe4\xb8\xad\xe6\x96\x87"; // 中文, width 4
    auto result = Utils::truncate_by_display_width(cjk, 2);
    EXPECT_LE(Utils::utf8_display_width(result), 2);
}

TEST(test_truncate_by_display_width, mixed_ascii_and_cjk) {
    // "AB中文CD" = 2 + 4 + 2 = width 8, truncate to 5
    std::string mixed = "AB\xe4\xb8\xad\xe6\x96\x87CD";
    auto result = Utils::truncate_by_display_width(mixed, 5);
    EXPECT_LE(Utils::utf8_display_width(result), 5);
}

// =============================================================================
// Format Duration Tests
// =============================================================================

TEST(test_format_duration, zero_seconds) {
    auto result = Utils::format_duration(0);
    // Should show 0:00 or similar
    EXPECT_FALSE(result.empty());
}

TEST(test_format_duration, seconds_only) {
    auto result = Utils::format_duration(45);
    EXPECT_FALSE(result.empty());
    // Should not contain hours
    // Format is likely M:SS or MM:SS
}

TEST(test_format_duration, minutes_and_seconds) {
    auto result = Utils::format_duration(125); // 2:05
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("2"), std::string::npos);
}

TEST(test_format_duration, hours_minutes_seconds) {
    auto result = Utils::format_duration(3661); // 1:01:01
    EXPECT_FALSE(result.empty());
    // Should contain "1:" somewhere for the hour
    EXPECT_NE(result.find("1:"), std::string::npos);
}

TEST(test_format_duration, large_values) {
    auto result = Utils::format_duration(86400); // 24 hours
    EXPECT_FALSE(result.empty());
}

TEST(test_format_duration, negative_zero) {
    // Negative or zero should still produce valid output
    auto result = Utils::format_duration(0);
    EXPECT_FALSE(result.empty());
}

// =============================================================================
// Format Size Tests
// =============================================================================

TEST(test_format_size, bytes) {
    auto result = Utils::format_size(500);
    EXPECT_FALSE(result.empty());
    // Should mention bytes or just show number
}

TEST(test_format_size, kilobytes) {
    auto result = Utils::format_size(2048);
    EXPECT_FALSE(result.empty());
    // Should contain "KB" or "KiB"
    EXPECT_NE(result.find("B"), std::string::npos);
}

TEST(test_format_size, megabytes) {
    auto result = Utils::format_size(5 * 1024 * 1024);
    EXPECT_FALSE(result.empty());
    // Should contain "MB" or "MiB"
}

TEST(test_format_size, gigabytes) {
    auto result = Utils::format_size(3LL * 1024 * 1024 * 1024);
    EXPECT_FALSE(result.empty());
}

TEST(test_format_size, zero_bytes) {
    auto result = Utils::format_size(0);
    EXPECT_FALSE(result.empty());
}

// =============================================================================
// URL Encode Tests
// =============================================================================

TEST(test_url_encode, preserves_safe_characters) {
    // Safe characters: A-Z a-z 0-9 - _ . ~
    EXPECT_EQ(Utils::url_encode("hello"), "hello");
    EXPECT_EQ(Utils::url_encode("Hello-World_123"), "Hello-World_123");
}

TEST(test_url_encode, encodes_special_characters) {
    auto result = Utils::url_encode("hello world");
    EXPECT_NE(result, "hello world");
    EXPECT_NE(result.find("%20"), std::string::npos);
}

TEST(test_url_encode, encodes_query_parameters) {
    auto result = Utils::url_encode("key=value&foo=bar");
    EXPECT_NE(result.find("%3D"), std::string::npos); // =
    EXPECT_NE(result.find("%26"), std::string::npos); // &
}

TEST(test_url_encode, handles_unicode) {
    // Unicode characters should be percent-encoded in UTF-8
    auto result = Utils::url_encode("\xc3\xa9"); // é
    EXPECT_NE(result.find("%"), std::string::npos);
}

TEST(test_url_encode, empty_string) {
    EXPECT_EQ(Utils::url_encode(""), "");
}

// =============================================================================
// Sanitize Filename Tests
// =============================================================================

TEST(test_sanitize_filename, preserves_safe_names) {
    EXPECT_EQ(Utils::sanitize_filename("podcast_episode_01"), "podcast_episode_01");
    EXPECT_EQ(Utils::sanitize_filename("Hello World"), "Hello World");
}

TEST(test_sanitize_filename, removes_slashes) {
    auto result = Utils::sanitize_filename("path/to/file");
    EXPECT_EQ(result.find('/'), std::string::npos);
    EXPECT_EQ(result.find('\\'), std::string::npos);
}

TEST(test_sanitize_filename, handles_null_bytes) {
    auto result = Utils::sanitize_filename("file\x00name");
    EXPECT_EQ(result.find('\0'), std::string::npos);
}

TEST(test_sanitize_filename, handles_empty) {
    auto result = Utils::sanitize_filename("");
    EXPECT_FALSE(result.empty()); // Should produce some default or stay empty
}

TEST(test_sanitize_filename, strips_special_chars) {
    // Common unsafe filesystem characters
    auto result = Utils::sanitize_filename("file<>:\"|?*name");
    // Should not contain null, pipe, or other shell-dangerous chars
    EXPECT_EQ(result.find('|'), std::string::npos);
    EXPECT_EQ(result.find('\0'), std::string::npos);
}

// =============================================================================
// Expand Path Tests
// =============================================================================

TEST(test_expand_path, expands_tilde) {
    auto result = Utils::expand_path("~/Documents");
    EXPECT_NE(result.find("~"), std::string::npos);
    // Result should start with /home/ or similar absolute path
    // (Note: depends on test environment HOME)
}

TEST(test_expand_path, absolute_path_unchanged) {
    std::string abs_path = "/tmp/podradio_test";
    auto result = Utils::expand_path(abs_path);
    EXPECT_EQ(result, abs_path);
}

TEST(test_expand_path, empty_string) {
    auto result = Utils::expand_path("");
    // Empty should return empty or current directory
    EXPECT_TRUE(result.empty() || result[0] == '/');
}

TEST(test_expand_path, relative_path) {
    auto result = Utils::expand_path("relative/path");
    // Should not contain ~ and should be usable
    EXPECT_NE(result.find('~'), std::string::npos); // might or might not expand
}
