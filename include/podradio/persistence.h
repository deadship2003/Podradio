#pragma once

#include "common.h"
#include "podradio/online_state.h"

namespace podradio {

// =========================================================
// Persistence - Save/load application data
// Manages radio cache, podcast subscriptions, favourites,
// OPML export/import, and data migration from legacy files.
// =========================================================
class Persistence {
public:
    // Save RADIO tree to database
    static void save_cache(const TreeNodePtr& radio, const TreeNodePtr& podcast);

    // Load RADIO tree from database
    static void load_cache(TreeNodePtr& radio, TreeNodePtr& podcast);

    // Save subscriptions and favourites to database
    // Key fix: clears nodes table before saving, so deletions persist
    static void save_data(const std::vector<TreeNodePtr>& podcasts,
                          const std::vector<TreeNodePtr>& favs);

    // Load subscriptions and favourites from database
    static void load_data(std::vector<TreeNodePtr>& podcasts,
                          std::vector<TreeNodePtr>& favs);

    // Export podcasts to OPML file
    static void export_opml(const std::string& filename, const std::vector<TreeNodePtr>& podcasts);

private:
    // Serialize a tree node to JSON
    static json save_tree(const TreeNodePtr& node);

    // Deserialize a tree node from JSON
    static void load_tree(TreeNodePtr& node, const json& j);

    // Serialize a node (minimal, for migration)
    static json save_node(const TreeNodePtr& node);

    // Deserialize a node (minimal, for migration)
    static TreeNodePtr load_node(const json& j);

    // Migrate data from legacy data.json to database
    static void migrate_from_json(std::vector<TreeNodePtr>& podcasts,
                                  std::vector<TreeNodePtr>& favs);
};

} // namespace podradio
