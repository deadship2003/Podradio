// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                    PodRadio Unit Tests - Database                         ║
// ║              Tests for SQLite persistence layer                           ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#include <gtest/gtest.h>
#include "podradio/database.h"

#include <filesystem>
#include <fstream>

using namespace podradio;

// =============================================================================
// Test fixture - creates a temporary database for each test
// =============================================================================
class DatabaseTest : public ::testing::Test {
protected:
    std::string test_db_path;

    void SetUp() override {
        // Create a temporary directory for the test database
        test_db_path = std::filesystem::temp_directory_path() / "podradio_test_XXXXXX.db";
        // Use mkstemp to get a unique filename
        char tmpl[256];
        std::strncpy(tmpl, test_db_path.c_str(), sizeof(tmpl) - 1);
        tmpl[sizeof(tmpl) - 1] = '\0';
        int fd = mkstemp(tmpl);
        if (fd >= 0) {
            close(fd);
            test_db_path = tmpl;
        }

        // The DatabaseManager uses a singleton that opens ~/.local/share/podradio/podradio.db
        // For testing, we init and then purge all data between tests
        auto& db = DatabaseManager::instance();
        db.init();
        db.purge_all();
    }

    void TearDown() override {
        auto& db = DatabaseManager::instance();
        db.purge_all();

        // Clean up temp file
        if (!test_db_path.empty() && std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }
    }
};

// =============================================================================
// Progress Tests
// =============================================================================

TEST_F(DatabaseTest, test_save_and_get_progress) {
    auto& db = DatabaseManager::instance();
    ASSERT_TRUE(db.is_ready());

    std::string url = "https://example.com/podcast/episode1.mp3";

    // Initially, progress should be at 0 and not completed
    auto [pos, completed] = db.get_progress(url);
    EXPECT_DOUBLE_EQ(pos, 0.0);
    EXPECT_FALSE(completed);

    // Save some progress
    db.save_progress(url, 123.45, false);

    // Retrieve and verify
    auto [pos2, completed2] = db.get_progress(url);
    EXPECT_DOUBLE_EQ(pos2, 123.45);
    EXPECT_FALSE(completed2);

    // Save as completed
    db.save_progress(url, 300.0, true);
    auto [pos3, completed3] = db.get_progress(url);
    EXPECT_DOUBLE_EQ(pos3, 300.0);
    EXPECT_TRUE(completed3);

    // Different URL should have no progress
    auto [pos4, completed4] = db.get_progress("https://example.com/other.mp3");
    EXPECT_DOUBLE_EQ(pos4, 0.0);
}

TEST_F(DatabaseTest, test_progress_overwrite) {
    auto& db = DatabaseManager::instance();

    std::string url = "https://example.com/overwrite_test.mp3";

    db.save_progress(url, 50.0, false);
    db.save_progress(url, 75.0, false);

    auto [pos, completed] = db.get_progress(url);
    EXPECT_DOUBLE_EQ(pos, 75.0);
    EXPECT_FALSE(completed);
}

// =============================================================================
// History Tests
// =============================================================================

TEST_F(DatabaseTest, test_add_and_get_history) {
    auto& db = DatabaseManager::instance();

    // Add history entries
    db.add_history("https://example.com/ep1.mp3", "Episode 1: Introduction", 1800);
    db.add_history("https://example.com/ep2.mp3", "Episode 2: Getting Started", 2400);

    // Retrieve history
    auto history = db.get_history(50);
    ASSERT_GE(history.size(), 2u);

    // Find our entries (order may vary)
    bool found_ep1 = false, found_ep2 = false;
    for (const auto& [url, title, timestamp] : history) {
        if (url == "https://example.com/ep1.mp3" && title == "Episode 1: Introduction") {
            found_ep1 = true;
        }
        if (url == "https://example.com/ep2.mp3" && title == "Episode 2: Getting Started") {
            found_ep2 = true;
        }
    }
    EXPECT_TRUE(found_ep1) << "Episode 1 not found in history";
    EXPECT_TRUE(found_ep2) << "Episode 2 not found in history";

    // Test limit parameter
    db.add_history("https://example.com/ep3.mp3", "Episode 3", 1200);
    auto limited = db.get_history(2);
    EXPECT_LE(limited.size(), 2u);
}

TEST_F(DatabaseTest, test_delete_history) {
    auto& db = DatabaseManager::instance();

    db.add_history("https://example.com/temp.mp3", "Temporary", 60);
    auto history = db.get_history(50);
    bool found = false;
    for (const auto& [url, title, ts] : history) {
        if (url == "https://example.com/temp.mp3") found = true;
    }
    EXPECT_TRUE(found);

    // Delete it
    db.delete_history("https://example.com/temp.mp3");
    auto history2 = db.get_history(50);
    found = false;
    for (const auto& [url, title, ts] : history2) {
        if (url == "https://example.com/temp.mp3") found = true;
    }
    EXPECT_FALSE(found);
}

// =============================================================================
// Favourites Tests
// =============================================================================

TEST_F(DatabaseTest, test_save_and_get_favourite) {
    auto& db = DatabaseManager::instance();

    db.save_favourite("Tech Podcast", "https://example.com/tech.xml",
                      1, false, "TechChannel", "rss", "{\"artwork\":\"url\"}");

    auto favourites = db.load_favourites();
    ASSERT_GE(favourites.size(), 1u);

    bool found = false;
    for (const auto& [title, url, type, is_yt, channel, source, data] : favourites) {
        if (url == "https://example.com/tech.xml") {
            EXPECT_EQ(title, "Tech Podcast");
            EXPECT_EQ(type, 1);
            EXPECT_FALSE(is_yt);
            EXPECT_EQ(channel, "TechChannel");
            EXPECT_EQ(source, "rss");
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Saved favourite not found in load_favourites()";
}

TEST_F(DatabaseTest, test_is_favourite) {
    auto& db = DatabaseManager::instance();

    std::string url = "https://example.com/fav_test.mp3";

    // Not a favourite initially
    EXPECT_FALSE(db.is_favourite(url));

    // Save as favourite
    db.save_favourite("Test Favourite", url, 0);

    // Now it should be a favourite
    EXPECT_TRUE(db.is_favourite(url));

    // Different URL should not be a favourite
    EXPECT_FALSE(db.is_favourite("https://example.com/not_a_fav.mp3"));
}

TEST_F(DatabaseTest, test_delete_favourite) {
    auto& db = DatabaseManager::instance();

    std::string url = "https://example.com/to_delete.xml";

    db.save_favourite("Delete Me", url, 1);
    EXPECT_TRUE(db.is_favourite(url));

    db.delete_favourite(url);
    EXPECT_FALSE(db.is_favourite(url));

    // load_favourites should not contain it
    auto favourites = db.load_favourites();
    for (const auto& [title, fav_url, type, is_yt, ch, src, data] : favourites) {
        EXPECT_NE(fav_url, url);
    }
}

TEST_F(DatabaseTest, test_clear_favourites) {
    auto& db = DatabaseManager::instance();

    db.save_favourite("Fav1", "https://example.com/fav1.xml", 1);
    db.save_favourite("Fav2", "https://example.com/fav2.xml", 1);

    EXPECT_GE(db.load_favourites().size(), 2u);

    db.clear_favourites();

    EXPECT_EQ(db.load_favourites().size(), 0u);
}

// =============================================================================
// URL Data Cache Tests
// =============================================================================

TEST_F(DatabaseTest, test_cache_url_data) {
    auto& db = DatabaseManager::instance();

    std::string url = "https://example.com/api/data.json";
    std::string json_data = R"({"key":"value","items":[1,2,3]})";

    // Initially no cached data
    auto cached = db.get_cached_data(url);
    EXPECT_TRUE(cached.empty());

    // Cache data
    db.cache_url_data(url, json_data);

    // Retrieve cached data
    auto retrieved = db.get_cached_data(url);
    EXPECT_EQ(retrieved, json_data);

    // Different URL should not return data
    auto other = db.get_cached_data("https://example.com/other.json");
    EXPECT_TRUE(other.empty());
}

TEST_F(DatabaseTest, test_cache_url_data_overwrite) {
    auto& db = DatabaseManager::instance();

    std::string url = "https://example.com/overwrite.json";

    db.cache_url_data(url, R"({"v":1})");
    db.cache_url_data(url, R"({"v":2,"updated":true})");

    auto retrieved = db.get_cached_data(url);
    EXPECT_EQ(retrieved, R"({"v":2,"updated":true})");
}

TEST_F(DatabaseTest, test_cache_url_data_large_payload) {
    auto& db = DatabaseManager::instance();

    // Build a large JSON payload
    std::string large_data(10000, 'x');
    large_data = R"({"data":")" + large_data + R"("})";

    db.cache_url_data("https://example.com/large.json", large_data);

    auto retrieved = db.get_cached_data("https://example.com/large.json");
    EXPECT_EQ(retrieved, large_data);
    EXPECT_GT(retrieved.size(), 10000u);
}
