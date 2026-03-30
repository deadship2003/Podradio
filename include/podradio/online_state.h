#pragma once

#include "common.h"
#include "podradio/itunes_search.h"
#include "podradio/database.h"

namespace podradio {

// =========================================================
// Online State - Manages online search mode state
// Singleton pattern. Handles search history, region cycling,
// and result tree management.
// =========================================================
class OnlineState {
public:
    static OnlineState& instance();

    // Current search region (default "US")
    std::string current_region = "US";

    // Last search query
    std::string last_query;

    // Current search results
    std::vector<TreeNodePtr> search_results;

    // Root node for online mode tree
    TreeNodePtr online_root = std::make_shared<TreeNode>();

    // Whether search history has been loaded
    bool history_loaded = false;

    // Set current search region
    void set_region(const std::string& region);

    // Cycle to next region, return new region name
    std::string get_next_region();

    // Load search history from database and build node tree
    void load_search_history();

    // Add or update search node (called after search)
    void add_or_update_search_node(const std::string& query, const std::string& region,
                                   const std::vector<TreeNodePtr>& results);

    // Clear current search results (keep history)
    void clear_results();

    // Force reload history from database
    void reload_history();

private:
    OnlineState();

    // Create a search history node from database item
    TreeNodePtr create_search_node(const SearchHistoryItem& item);

    // Update search node title with formatted info
    void update_search_node_title(TreeNodePtr node, const std::string& query,
                                  const std::string& region, int count);

    // Generate unique search identifier
    static std::string make_search_id(const std::string& query, const std::string& region);

    // Format database timestamp
    static std::string format_timestamp(const std::string& timestamp);

    // Get current time string
    static std::string get_current_time_str();
};

} // namespace podradio
