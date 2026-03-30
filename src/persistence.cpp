#include "podradio/persistence.h"
#include "podradio/logger.h"

namespace podradio
{

void Persistence::save_cache(const TreeNodePtr& radio, const TreeNodePtr& /*podcast*/) {
    // Save RADIO tree to database
    if (radio) {
        DatabaseManager::instance().save_radio_cache(radio);
    }
    // PODCAST tree is saved via save_data() to nodes table
}

void Persistence::load_cache(TreeNodePtr& radio, TreeNodePtr& /*podcast*/) {
    // Load RADIO tree from database
    radio = DatabaseManager::instance().load_radio_cache();
    // PODCAST tree is loaded via load_data() from nodes table
}

void Persistence::save_data(const std::vector<TreeNodePtr>& podcasts,
                            const std::vector<TreeNodePtr>& favs) {
    // Clear nodes table first
    DatabaseManager::instance().clear_nodes();

    // Save subscriptions to nodes table
    for (const auto& p : podcasts) {
        if (p && !p->url.empty()) {
            json data_json;
            data_json["expanded"] = p->expanded;
            data_json["children_loaded"] = p->children_loaded;
            data_json["is_youtube"] = p->is_youtube;
            data_json["channel_name"] = p->channel_name;
            data_json["is_cached"] = p->is_cached;
            data_json["is_downloaded"] = p->is_downloaded;
            data_json["local_file"] = p->local_file;

            // Save child node info
            if (!p->children.empty()) {
                data_json["children"] = json::array();
                for (const auto& c : p->children) {
                    json child_json;
                    child_json["title"] = c->title;
                    child_json["url"] = c->url;
                    child_json["type"] = (int)c->type;
                    child_json["duration"] = c->duration;
                    data_json["children"].push_back(child_json);
                }
            }

            DatabaseManager::instance().save_node(
                p->url, p->title, (int)p->type, data_json.dump()
            );
        }
    }

    // Clear favourites table first
    DatabaseManager::instance().clear_favourites();

    // Save favourites to favourites table
    for (const auto& f : favs) {
        if (f) {
            json data_json;
            data_json["children_loaded"] = f->children_loaded;
            if (!f->children.empty()) {
                data_json["children"] = json::array();
                for (const auto& c : f->children) {
                    json child_json;
                    child_json["title"] = c->title;
                    child_json["url"] = c->url;
                    child_json["type"] = (int)c->type;
                    data_json["children"].push_back(child_json);
                }
            }

            DatabaseManager::instance().save_favourite(
                f->title,
                f->url,
                (int)f->type,
                f->is_youtube,
                f->channel_name,
                "",  // source_type
                data_json.dump()
            );
        }
    }
}

void Persistence::load_data(std::vector<TreeNodePtr>& podcasts,
                            std::vector<TreeNodePtr>& favs) {
    // First try loading subscriptions from database nodes table
    auto db_podcasts = DatabaseManager::instance().load_nodes();
    if (!db_podcasts.empty()) {
        // Successfully loaded from database, no migration needed
        for (const auto& [title, url, type, data_json] : db_podcasts) {
            auto node = std::make_shared<TreeNode>();
            node->title = title;
            node->url = url;
            node->type = (NodeType)type;

            // Parse data_json
            if (!data_json.empty()) {
                try {
                    json j = json::parse(data_json);
                    node->expanded = j.value("expanded", false);
                    node->children_loaded = j.value("children_loaded", false);
                    node->is_youtube = j.value("is_youtube", false);
                    node->channel_name = j.value("channel_name", "");
                    node->is_cached = j.value("is_cached", false);
                    node->is_downloaded = j.value("is_downloaded", false);
                    node->local_file = j.value("local_file", "");

                    // Load child nodes
                    if (j.contains("children") && j["children"].is_array()) {
                        for (const auto& c : j["children"]) {
                            auto child = std::make_shared<TreeNode>();
                            child->title = c.value("title", "");
                            child->url = c.value("url", "");
                            child->type = (NodeType)c.value("type", 0);
                            child->duration = c.value("duration", 0);
                            child->children_loaded = true;
                            child->parent = node;
                            node->children.push_back(child);
                        }
                    }
                } catch (...) {}
            }

            podcasts.push_back(node);
        }
    } else {
        // Database empty, try migrating from legacy data.json
        migrate_from_json(podcasts, favs);
    }

    // Load favourites from database
    auto db_favs = DatabaseManager::instance().load_favourites();
    for (const auto& [title, url, type, is_youtube, channel_name, source_type, data_json] : db_favs) {
        auto node = std::make_shared<TreeNode>();
        node->title = title;
        node->url = url;
        node->type = (NodeType)type;
        node->is_youtube = is_youtube;
        node->channel_name = channel_name;
        node->children_loaded = true;

        // Fix LINK loading - parse LINK info from data_json
        // When saving favourites, data_json contains: {"is_link":true, "source_mode":"RADIO", "link_target_url":"..."}
        if (!data_json.empty()) {
            try {
                json j = json::parse(data_json);

                // Key fix: restore is_link field from data_json
                if (j.value("is_link", false)) {
                    node->is_link = true;
                    node->source_mode = j.value("source_mode", source_type);
                    node->link_target_url = j.value("link_target_url", url);
                    node->children_loaded = false;  // LINK nodes load lazily

                    // Special case: online_root
                    if (url == "online_root" || node->link_target_url == "online_root") {
                        node->linked_node = OnlineState::instance().online_root;
                    }
                } else {
                    // Non-LINK nodes: parse children
                    node->children_loaded = j.value("children_loaded", true);
                    if (j.contains("children") && j["children"].is_array()) {
                        for (const auto& c : j["children"]) {
                            auto child = std::make_shared<TreeNode>();
                            child->title = c.value("title", "");
                            child->url = c.value("url", "");
                            child->type = (NodeType)c.value("type", 0);
                            child->children_loaded = true;
                            child->parent = node;
                            node->children.push_back(child);
                        }
                    }
                }
            } catch (...) {
                // Parse failed, use old source_type logic as fallback
                if (source_type == "online_root_link" || url == "online_root") {
                    node->is_link = true;
                    node->linked_node = OnlineState::instance().online_root;
                    node->children_loaded = false;
                } else if (source_type == "search_record_link" && url.find("search:") == 0) {
                    node->is_link = true;
                    node->children_loaded = false;
                }
            }
        }

        favs.push_back(node);
    }
}

void Persistence::export_opml(const std::string& filename, const std::vector<TreeNodePtr>& podcasts) {
    std::ofstream f(filename);
    if (!f.is_open()) {
        std::cout << "Error: Cannot open file for writing: " << filename << std::endl;
        return;
    }
    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    f << "<opml version=\"1.0\">\n";
    f << "<head><title>PODRADIO Export</title></head>\n";
    f << "<body>\n";
    int count = 0;
    for (const auto& p : podcasts) {
        if (p && !p->url.empty()) {
            f << fmt::format("  <outline text=\"{}\" title=\"{}\" type=\"rss\" xmlUrl=\"{}\"/>\n",
                            p->title, p->title, p->url);
            count++;
        }
    }
    f << "</body>\n</opml>\n";
    f.close();
    std::cout << "Exported " << count << " podcasts to " << filename << std::endl;
}

json Persistence::save_tree(const TreeNodePtr& node) {
    json j = {{"title", node->title}, {"url", node->url}, {"type", (int)node->type},
              {"expanded", node->expanded}, {"children_loaded", node->children_loaded},
              {"is_youtube", node->is_youtube}, {"channel_name", node->channel_name},
              {"is_cached", node->is_cached}, {"is_downloaded", node->is_downloaded},
              {"local_file", node->local_file}};
    j["children"] = json::array();
    for (const auto& c : node->children) j["children"].push_back(save_tree(c));
    return j;
}

void Persistence::load_tree(TreeNodePtr& node, const json& j) {
    node->title = j.value("title", "");
    node->url = j.value("url", "");
    node->type = (NodeType)j.value("type", 0);
    node->expanded = j.value("expanded", false);
    node->children_loaded = j.value("children_loaded", false);
    node->is_youtube = j.value("is_youtube", false);
    node->channel_name = j.value("channel_name", "");
    node->is_cached = j.value("is_cached", false);
    node->is_downloaded = j.value("is_downloaded", false);
    node->local_file = j.value("local_file", "");
    if (j.contains("children")) for (const auto& c : j["children"]) {
        auto child = std::make_shared<TreeNode>();
        load_tree(child, c);
        child->parent = node;
        node->children.push_back(child);
    }
}

json Persistence::save_node(const TreeNodePtr& node) {
    return {{"title", node->title}, {"url", node->url}, {"type", (int)node->type},
            {"is_youtube", node->is_youtube}, {"is_cached", node->is_cached},
            {"is_downloaded", node->is_downloaded}, {"local_file", node->local_file}};
}

TreeNodePtr Persistence::load_node(const json& j) {
    auto node = std::make_shared<TreeNode>();
    node->title = j.value("title", "");
    node->url = j.value("url", "");
    node->type = (NodeType)j.value("type", 0);
    node->is_youtube = j.value("is_youtube", false);
    node->is_cached = j.value("is_cached", false);
    node->is_downloaded = j.value("is_downloaded", false);
    node->local_file = j.value("local_file", "");
    return node;
}

void Persistence::migrate_from_json(std::vector<TreeNodePtr>& podcasts,
                                    std::vector<TreeNodePtr>& favs) {
    const char* home = std::getenv("HOME");
    if (!home) return;

    std::string path = std::string(home) + DATA_DIR + "/data.json";
    if (!fs::exists(path)) return;

    try {
        std::ifstream f(path);
        json j; f >> j;

        // Migrate subscriptions
        if (j.contains("podcasts")) {
            for (const auto& p : j["podcasts"]) {
                podcasts.push_back(load_node(p));
            }
        }

        // Migrate favourites
        if (j.contains("favourites")) {
            for (const auto& fav : j["favourites"]) {
                auto node = load_node(fav);
                favs.push_back(node);

                // Also save to database
                json data_json;
                data_json["children_loaded"] = node->children_loaded;
                if (!node->children.empty()) {
                    data_json["children"] = json::array();
                    for (const auto& c : node->children) {
                        json child_json;
                        child_json["title"] = c->title;
                        child_json["url"] = c->url;
                        child_json["type"] = (int)c->type;
                        data_json["children"].push_back(child_json);
                    }
                }

                DatabaseManager::instance().save_favourite(
                    node->title, node->url, (int)node->type,
                    node->is_youtube, node->channel_name, "", data_json.dump()
                );
            }
        }

        // Rename old file after migration
        std::string backup_path = path + ".migrated";
        fs::rename(path, backup_path);
        LOG(fmt::format("[Migration] data.json migrated to database, backup saved to {}", backup_path));

    } catch (...) {}
}

} // namespace podradio
