#include "podradio/itunes_search.h"
#include "podradio/database.h"
#include "podradio/network.h"
#include "podradio/utils.h"
#include "podradio/logger.h"
#include "podradio/event_log.h"

namespace podradio
{

ITunesSearch& ITunesSearch::instance() {
    static ITunesSearch is;
    return is;
}

std::vector<std::string> ITunesSearch::get_regions() {
    return {"US", "CN", "TW", "JP", "UK", "DE", "FR", "KR", "AU"};
}

std::string ITunesSearch::get_region_name(const std::string& region) {
    static std::map<std::string, std::string> names = {
        {"US", "United States"}, {"CN", "China"}, {"TW", "Taiwan"}, {"JP", "Japan"},
        {"UK", "United Kingdom"}, {"DE", "Germany"}, {"FR", "France"}, {"KR", "South Korea"}, {"AU", "Australia"}
    };
    return names.count(region) ? names[region] : region;
}

std::vector<TreeNodePtr> ITunesSearch::search(const std::string& query, const std::string& region, int limit) {
    std::vector<TreeNodePtr> results;

    // Check cache first
    std::string cached = DatabaseManager::instance().load_search_cache(query, region);
    if (!cached.empty()) {
        try {
            json j = json::parse(cached);
            if (j.contains("results") && j["results"].is_array()) {
                for (const auto& item : j["results"]) {
                    auto node = parse_result(item);
                    if (node) {
                        // Check if already in podcast_cache
                        node->is_db_cached = DatabaseManager::instance().is_podcast_cached(node->url);
                        results.push_back(node);
                    }
                }
            }
            EVENT_LOG(fmt::format("[iTunes] Loaded from cache: {} results for '{}'", results.size(), query));
            return results;
        } catch (...) {}
    }

    // Network request
    std::string url = fmt::format(
        "https://itunes.apple.com/search?term={}&media=podcast&country={}&limit={}",
        Utils::url_encode(query), region, limit
    );

    std::string response = fetch(url);
    if (response.empty()) {
        EVENT_LOG(fmt::format("[iTunes] Search failed for '{}'", query));
        return results;
    }

    // Save to search cache
    DatabaseManager::instance().save_search_cache(query, region, response);

    // Parse results and save to podcast_cache
    try {
        json j = json::parse(response);
        if (j.contains("results") && j["results"].is_array()) {
            for (const auto& item : j["results"]) {
                auto node = parse_result(item);
                if (node) {
                    // Save to podcast_cache
                    save_podcast_to_cache(item);
                    // Check if cached
                    node->is_db_cached = DatabaseManager::instance().is_podcast_cached(node->url);
                    results.push_back(node);
                }
            }
        }
    } catch (const std::exception& e) {
        LOG(fmt::format("[iTunes] Parse error: {}", e.what()));
    }

    EVENT_LOG(fmt::format("[iTunes] Search '{}': {} results", query, results.size()));
    return results;
}

TreeNodePtr ITunesSearch::parse_result(const json& item) {
    auto node = std::make_shared<TreeNode>();
    node->type = NodeType::PODCAST_FEED;

    // Extract info from iTunes result
    if (item.contains("collectionName")) {
        node->title = item["collectionName"].get<std::string>();
    } else if (item.contains("trackName")) {
        node->title = item["trackName"].get<std::string>();
    } else {
        return nullptr;
    }

    if (item.contains("feedUrl")) {
        node->url = item["feedUrl"].get<std::string>();
    } else {
        return nullptr;  // Skip items without feed URL
    }

    // Build subtext with additional info
    std::string subtext;
    if (item.contains("artistName")) {
        subtext = item["artistName"].get<std::string>();
    }
    if (item.contains("trackCount")) {
        if (!subtext.empty()) subtext += " \xc2\xb7 ";
        subtext += std::to_string(item["trackCount"].get<int>()) + " episodes";
    }
    if (item.contains("primaryGenreName")) {
        if (!subtext.empty()) subtext += " \xc2\xb7 ";
        subtext += item["primaryGenreName"].get<std::string>();
    }
    node->subtext = subtext;

    return node;
}

void ITunesSearch::save_podcast_to_cache(const json& item) {
    if (!item.contains("feedUrl")) return;

    std::string feed_url = item["feedUrl"].get<std::string>();
    std::string title = item.value("collectionName", "");
    std::string artist = item.value("artistName", "");
    std::string genre = item.value("primaryGenreName", "");
    int track_count = item.value("trackCount", 0);
    std::string artwork_url = item.value("artworkUrl600", item.value("artworkUrl100", ""));
    int collection_id = item.value("collectionId", 0);

    DatabaseManager::instance().save_podcast_cache(
        feed_url, title, artist, genre, track_count, artwork_url, collection_id, item.dump()
    );
}

std::string ITunesSearch::fetch(const std::string& url) {
    // Use Network::fetch() instead of direct CURL calls
    return Network::fetch(url, 15);
}

} // namespace podradio
