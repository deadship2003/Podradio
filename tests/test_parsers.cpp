// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                    PodRadio Unit Tests - Parsers                          ║
// ║              Tests for URL classifier and feed parsers                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#include <gtest/gtest.h>
#include "podradio/url_classifier.h"

using namespace podradio;

// =============================================================================
// URL Classifier - YouTube Tests
// =============================================================================

TEST(test_url_classifier_youtube, youtube_channel_at) {
    EXPECT_EQ(URLClassifier::classify("https://www.youtube.com/@SomeChannel"),
              URLType::YOUTUBE_CHANNEL);
}

TEST(test_url_classifier_youtube, youtube_channel_custom) {
    EXPECT_EQ(URLClassifier::classify("https://www.youtube.com/c/CustomChannel"),
              URLType::YOUTUBE_CHANNEL);
}

TEST(test_url_classifier_youtube, youtube_channel_id) {
    EXPECT_EQ(URLClassifier::classify("https://www.youtube.com/channel/UCxxxxxxxxxxxxxx"),
              URLType::YOUTUBE_CHANNEL);
}

TEST(test_url_classifier_youtube, youtube_watch) {
    EXPECT_EQ(URLClassifier::classify("https://www.youtube.com/watch?v=dQw4w9WgXcQ"),
              URLType::YOUTUBE_VIDEO);
}

TEST(test_url_classifier_youtube, youtube_short_link) {
    EXPECT_EQ(URLClassifier::classify("https://youtu.be/dQw4w9WgXcQ"),
              URLType::YOUTUBE_VIDEO);
}

TEST(test_url_classifier_youtube, youtube_playlist) {
    EXPECT_EQ(URLClassifier::classify("https://www.youtube.com/playlist?list=PLxxxxx"),
              URLType::YOUTUBE_PLAYLIST);
}

TEST(test_url_classifier_youtube, youtube_rss_feed) {
    EXPECT_EQ(URLClassifier::classify("https://www.youtube.com/feeds/videos.xml?channel_id=UCxxx"),
              URLType::YOUTUBE_RSS);
}

TEST(test_url_classifier_youtube, is_youtube_helper) {
    EXPECT_TRUE(URLClassifier::is_youtube(URLType::YOUTUBE_CHANNEL));
    EXPECT_TRUE(URLClassifier::is_youtube(URLType::YOUTUBE_VIDEO));
    EXPECT_TRUE(URLClassifier::is_youtube(URLType::YOUTUBE_RSS));
    EXPECT_TRUE(URLClassifier::is_youtube(URLType::YOUTUBE_PLAYLIST));
    EXPECT_FALSE(URLClassifier::is_youtube(URLType::RSS_PODCAST));
    EXPECT_FALSE(URLClassifier::is_youtube(URLType::RADIO_STREAM));
    EXPECT_FALSE(URLClassifier::is_youtube(URLType::UNKNOWN));
}

TEST(test_url_classifier_youtube, extract_channel_name_at) {
    EXPECT_EQ(URLClassifier::extract_channel_name("https://www.youtube.com/@MyChannel"),
              "MyChannel");
}

TEST(test_url_classifier_youtube, extract_channel_name_custom) {
    EXPECT_EQ(URLClassifier::extract_channel_name("https://www.youtube.com/c/CustomName"),
              "CustomName");
}

TEST(test_url_classifier_youtube, extract_channel_name_with_query) {
    // Channel name extraction should stop at query params
    auto name = URLClassifier::extract_channel_name("https://www.youtube.com/@Channel?param=val");
    EXPECT_EQ(name, "Channel");
}

// =============================================================================
// URL Classifier - RSS Tests
// =============================================================================

TEST(test_url_classifier_rss, rss_xml_extension) {
    EXPECT_EQ(URLClassifier::classify("https://feeds.example.com/podcast.xml"),
              URLType::RSS_PODCAST);
}

TEST(test_url_classifier_rss, rss_feed_path) {
    EXPECT_EQ(URLClassifier::classify("https://example.com/feed"),
              URLType::RSS_PODCAST);
}

TEST(test_url_classifier_rss, rss_rss_path) {
    EXPECT_EQ(URLClassifier::classify("https://example.com/rss"),
              URLType::RSS_PODCAST);
}

TEST(test_url_classifier_rss, apple_podcast) {
    EXPECT_EQ(URLClassifier::classify("https://podcasts.apple.com/us/podcast/id123456"),
              URLType::APPLE_PODCAST);
}

TEST(test_url_classifier_rss, apple_podcast_not_rss) {
    // Apple Podcast URLs should NOT be classified as generic RSS
    auto type = URLClassifier::classify("https://podcasts.apple.com/us/podcast/id123456");
    EXPECT_EQ(type, URLType::APPLE_PODCAST);
    EXPECT_NE(type, URLType::RSS_PODCAST);
}

// =============================================================================
// URL Classifier - OPML Tests
// =============================================================================

TEST(test_url_classifier_opml, opml_extension) {
    EXPECT_EQ(URLClassifier::classify("https://example.com/stations.opml"),
              URLType::OPML);
}

TEST(test_url_classifier_opml, browse_ashx) {
    EXPECT_EQ(URLClassifier::classify("https://opml.radiotime.com/Browse.ashx?id=r0"),
              URLType::OPML);
}

TEST(test_url_classifier_opml, tune_ashx_browse) {
    EXPECT_EQ(URLClassifier::classify("https://opml.radiotime.com/Tune.ashx?id=s1234&c=pbrowse"),
              URLType::OPML);
}

TEST(test_url_classifier_opml, tune_ashx_stream) {
    // Tune.ashx without browse category should be radio stream
    EXPECT_EQ(URLClassifier::classify("https://opml.radiotime.com/Tune.ashx?id=s1234"),
              URLType::RADIO_STREAM);
}

TEST(test_url_classifier_opml, tune_ashx_sbrowse) {
    EXPECT_EQ(URLClassifier::classify("https://opml.radiotime.com/Tune.ashx?c=sbrowse"),
              URLType::OPML);
}

// =============================================================================
// URL Classifier - Other Types
// =============================================================================

TEST(test_url_classifier, empty_url) {
    EXPECT_EQ(URLClassifier::classify(""), URLType::UNKNOWN);
}

TEST(test_url_classifier, local_file) {
    EXPECT_EQ(URLClassifier::classify("/home/user/music.mp3"), URLType::RADIO_STREAM);
    EXPECT_EQ(URLClassifier::classify("file:///home/user/music.mp3"), URLType::RADIO_STREAM);
}

TEST(test_url_classifier, audio_extensions) {
    EXPECT_EQ(URLClassifier::classify("https://stream.example.com/live.m3u8"), URLType::RADIO_STREAM);
    EXPECT_EQ(URLClassifier::classify("https://example.com/audio.mp3"), URLType::RADIO_STREAM);
    EXPECT_EQ(URLClassifier::classify("https://example.com/audio.aac"), URLType::RADIO_STREAM);
}

TEST(test_url_classifier, video_extensions) {
    EXPECT_EQ(URLClassifier::classify("https://example.com/video.mp4"), URLType::VIDEO_FILE);
    EXPECT_EQ(URLClassifier::classify("https://example.com/video.mkv"), URLType::VIDEO_FILE);
    EXPECT_EQ(URLClassifier::classify("https://example.com/video.webm"), URLType::VIDEO_FILE);
    EXPECT_EQ(URLClassifier::classify("https://example.com/video.avi"), URLType::VIDEO_FILE);
    EXPECT_EQ(URLClassifier::classify("https://example.com/video.mov"), URLType::VIDEO_FILE);
}

TEST(test_url_classifier, type_name) {
    EXPECT_EQ(URLClassifier::type_name(URLType::UNKNOWN), "Unknown");
    EXPECT_EQ(URLClassifier::type_name(URLType::OPML), "OPML");
    EXPECT_EQ(URLClassifier::type_name(URLType::RSS_PODCAST), "RSS");
    EXPECT_EQ(URLClassifier::type_name(URLType::YOUTUBE_CHANNEL), "YouTube Channel");
    EXPECT_EQ(URLClassifier::type_name(URLType::YOUTUBE_VIDEO), "YouTube Video");
    EXPECT_EQ(URLClassifier::type_name(URLType::YOUTUBE_RSS), "YouTube RSS");
    EXPECT_EQ(URLClassifier::type_name(URLType::YOUTUBE_PLAYLIST), "YouTube Video"); // Note: not explicitly in type_name
    EXPECT_EQ(URLClassifier::type_name(URLType::APPLE_PODCAST), "Apple Podcast");
    EXPECT_EQ(URLClassifier::type_name(URLType::RADIO_STREAM), "Radio Stream");
    EXPECT_EQ(URLClassifier::type_name(URLType::VIDEO_FILE), "Video File");
}

TEST(test_url_classifier, is_video) {
    EXPECT_TRUE(URLClassifier::is_video(URLType::YOUTUBE_CHANNEL));
    EXPECT_TRUE(URLClassifier::is_video(URLType::YOUTUBE_VIDEO));
    EXPECT_TRUE(URLClassifier::is_video(URLType::VIDEO_FILE));
    EXPECT_FALSE(URLClassifier::is_video(URLType::RADIO_STREAM));
    EXPECT_FALSE(URLClassifier::is_video(URLType::RSS_PODCAST));
    EXPECT_FALSE(URLClassifier::is_video(URLType::OPML));
}
