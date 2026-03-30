#pragma once

#include "common.h"

namespace podradio {

// =========================================================
// OPML Parser
// Parses OPML format files for radio stream browsing and import.
// =========================================================
class OPMLParser {
public:
    // Parse OPML XML string into a TreeNode tree
    static TreeNodePtr parse(const std::string& xml);

    // Import OPML file, extract all RSS feed subscriptions
    static std::vector<TreeNodePtr> import_opml_file(const std::string& filepath);

private:
    // Parse outline nodes recursively
    static void parse_outline(xmlNodePtr node, TreeNodePtr parent, bool is_top_level = false);

    // Extract RSS feeds from OPML XML
    static void extract_feeds(const std::string& xml, std::vector<TreeNodePtr>& feeds);
};

// =========================================================
// RSS Parser
// Parses RSS 2.0, Atom, YouTube Atom feeds.
// Supports iTunes extensions, Media RSS, Podcast 2.0.
// =========================================================
class RSSParser {
public:
    // Parse RSS/Atom XML string into a podcast feed TreeNode
    static TreeNodePtr parse(std::string xml, const std::string& feed_url);

private:
    // Parse RSS 2.0 channel element
    static void parse_channel(xmlNodePtr ch, TreeNodePtr c);

    // Parse RSS 2.0 item element
    static TreeNodePtr parse_item(xmlNodePtr it);

    // Parse Atom feed
    static void parse_atom_feed(xmlNodePtr feed, TreeNodePtr c);

    // Parse Atom entry element
    static TreeNodePtr parse_atom_entry(xmlNodePtr entry);

    // Parse YouTube Atom feed (uses XPath)
    static void parse_youtube_atom(xmlDocPtr doc, TreeNodePtr c);

    // Parse YouTube Atom entry element
    static TreeNodePtr parse_youtube_entry(xmlNodePtr entry);
};

// =========================================================
// YouTube Channel Parser
// Uses yt-dlp to list videos from a YouTube channel.
// =========================================================
class YouTubeChannelParser {
public:
    // Parse a YouTube channel URL into a podcast feed TreeNode
    static TreeNodePtr parse(const std::string& channel_url);
};

} // namespace podradio
