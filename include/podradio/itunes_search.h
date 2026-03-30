#pragma once

#include "common.h"

namespace podradio {

// =========================================================
// iTunes Search - Search Apple Podcasts directory
// Singleton pattern. Supports region selection and search caching.
// =========================================================
class ITunesSearch {
public:
    static ITunesSearch& instance();

    // Get list of supported regions
    static std::vector<std::string> get_regions();

    // Get display name for a region code
    static std::string get_region_name(const std::string& region);

    // Search podcasts (with cache marking)
    std::vector<TreeNodePtr> search(const std::string& query, const std::string& region = "US", int limit = 50);

    // Parse a single iTunes search result item
    TreeNodePtr parse_result(const json& item);

private:
    ITunesSearch() {}

    // Save podcast info to cache
    void save_podcast_to_cache(const json& item);

    // Fetch URL using Network::fetch()
    std::string fetch(const std::string& url);
};

} // namespace podradio
