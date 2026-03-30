#include "podradio/online_state.h"
#include "podradio/event_log.h"

namespace podradio
{

OnlineState& OnlineState::instance() {
    static OnlineState os;
    return os;
}

OnlineState::OnlineState() {
    online_root->title = "Online Search";
    online_root->type = NodeType::FOLDER;
}

void OnlineState::set_region(const std::string& region) {
    current_region = region;
}

std::string OnlineState::get_next_region() {
    auto regions = ITunesSearch::get_regions();
    auto it = std::find(regions.begin(), regions.end(), current_region);
    if (it != regions.end() && ++it != regions.end()) {
        current_region = *it;
    } else {
        current_region = regions[0];
    }
    return current_region;
}

void OnlineState::load_search_history() {
    if (history_loaded) return;

    online_root->children.clear();
    auto history = DatabaseManager::instance().load_all_search_history();

    for (const auto& item : history) {
        auto node = create_search_node(item);
        if (node) {
            node->parent = online_root;
            online_root->children.push_back(node);
        }
    }

    history_loaded = true;
    EVENT_LOG(fmt::format("[Online] Loaded {} search history items", history.size()));
}

void OnlineState::add_or_update_search_node(const std::string& query, const std::string& region,
                                            const std::vector<TreeNodePtr>& results) {
    // Find existing node with same query and region
    TreeNodePtr existing_node = nullptr;
    for (auto& child : online_root->children) {
        if (child->url == make_search_id(query, region)) {
            existing_node = child;
            break;
        }
    }

    if (existing_node) {
        // Update existing node - move to front, update children
        existing_node->children.clear();
        for (const auto& result : results) {
            result->parent = existing_node;
            existing_node->children.push_back(result);
        }
        // Update title with result count and time
        update_search_node_title(existing_node, query, region, results.size());

        // Move to front (newest first)
        auto it = std::find(online_root->children.begin(), online_root->children.end(), existing_node);
        if (it != online_root->children.begin()) {
            online_root->children.erase(it);
            online_root->children.insert(online_root->children.begin(), existing_node);
        }
    } else {
        // Create new node and insert at front
        auto node = std::make_shared<TreeNode>();
        node->type = NodeType::FOLDER;
        node->url = make_search_id(query, region);
        update_search_node_title(node, query, region, results.size());

        for (const auto& result : results) {
            result->parent = node;
            node->children.push_back(result);
        }

        node->parent = online_root;
        online_root->children.insert(online_root->children.begin(), node);
    }
}

void OnlineState::clear_results() {
    search_results.clear();
    // Don't clear online_root->children, keep history
}

void OnlineState::reload_history() {
    history_loaded = false;
    load_search_history();
}

TreeNodePtr OnlineState::create_search_node(const SearchHistoryItem& item) {
    auto node = std::make_shared<TreeNode>();
    node->type = NodeType::FOLDER;
    node->url = make_search_id(item.query, item.region);

    std::string region_name = ITunesSearch::get_region_name(item.region);
    std::string time_str = format_timestamp(item.timestamp);

    // Format: search icon [time][region][count(4 digits)]["keyword"]
    node->title = fmt::format("\xf0\x9f\x94\x8d [{}][{}][{:4d}][\"{}\"]",
        time_str, region_name, item.result_count, item.query);

    // Children need to be loaded on expand from cache
    node->children_loaded = false;

    return node;
}

void OnlineState::update_search_node_title(TreeNodePtr node, const std::string& query,
                                           const std::string& region, int count) {
    std::string region_name = ITunesSearch::get_region_name(region);
    std::string time_str = get_current_time_str();

    // Format: search icon [time][region][count(4 digits)]["keyword"]
    node->title = fmt::format("\xf0\x9f\x94\x8d [{}][{}][{:4d}][\"{}\"]",
        time_str, region_name, count, query);
}

std::string OnlineState::make_search_id(const std::string& query, const std::string& region) {
    return fmt::format("search:{}:{}", region, query);
}

std::string OnlineState::format_timestamp(const std::string& timestamp) {
    // Input format: "2026-03-05 19:33:36"
    // Output format: "2026-03-05 19:33:36" (unchanged)
    if (timestamp.empty()) return "";

    // SQLite CURRENT_TIMESTAMP format: YYYY-MM-DD HH:MM:SS
    return timestamp;
}

std::string OnlineState::get_current_time_str() {
    auto now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}

} // namespace podradio
