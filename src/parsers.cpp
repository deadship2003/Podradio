#include "podradio/parsers.h"
#include "podradio/url_classifier.h"
#include "podradio/logger.h"
#include "podradio/event_log.h"
#include "podradio/youtube_cache.h"

namespace podradio
{

// =========================================================
// OPML Parser implementation
// =========================================================
TreeNodePtr OPMLParser::parse(const std::string& xml) {
    // Reset XML error state
    reset_xml_error_state();

    // Suppress XML errors/warnings from outputting to terminal
    xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.size(), "opml", NULL, XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
    if (!doc) {
        std::string last_err = get_last_xml_error();
        if (!last_err.empty()) {
            LOG(fmt::format("[OPML] Parse failed: {}", last_err));
        }
        return nullptr;
    }

    auto root = std::make_shared<TreeNode>();
    root->title = "Root";
    root->type = NodeType::FOLDER;

    xmlNodePtr rootNode = xmlDocGetRootElement(doc);
    if (rootNode) {
        for (xmlNodePtr n = rootNode->children; n; n = n->next) {
            if (xmlStrcmp(n->name, (const xmlChar*)"body") == 0) {
                parse_outline(n->children, root, true);
            }
        }
    }

    xmlFreeDoc(doc);
    return root;
}

std::vector<TreeNodePtr> OPMLParser::import_opml_file(const std::string& filepath) {
    std::vector<TreeNodePtr> feeds;

    if (!fs::exists(filepath)) return feeds;

    std::ifstream f(filepath);
    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string xml = buffer.str();

    if (xml.empty()) return feeds;

    // Parse OPML, extract all RSS subscriptions
    extract_feeds(xml, feeds);

    return feeds;
}

void OPMLParser::parse_outline(xmlNodePtr node, TreeNodePtr parent, bool /*is_top_level*/) {
    for (; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE || xmlStrcmp(node->name, (const xmlChar*)"outline") == 0) {
            // Skip non-outline elements
        }
        if (node->type != XML_ELEMENT_NODE || xmlStrcmp(node->name, (const xmlChar*)"outline") != 0) continue;

        auto child = std::make_shared<TreeNode>();
        child->title = get_xml_prop_any(node, {"text", "title"});
        child->parent = parent;

        std::string type = get_xml_prop_any(node, {"type"});
        std::string url = get_xml_prop_any(node, {"URL", "url"});
        std::string item = get_xml_prop_any(node, {"item"});
        std::string key = get_xml_prop_any(node, {"key"});
        std::string subtext = get_xml_prop_any(node, {"subtext"});
        std::string duration_str = get_xml_prop_any(node, {"topic_duration"});
        std::string stream_type = get_xml_prop_any(node, {"stream_type"});

        child->subtext = subtext;
        if (!duration_str.empty()) {
            try { child->duration = std::stoi(duration_str); } catch (...) {}
        }

        // stream_type for download mark detection, not implemented yet
        (void)stream_type;

        if (item == "topic" && type == "audio") {
            child->type = NodeType::PODCAST_EPISODE;
            child->url = url;
            child->children_loaded = true;
            parent->children.push_back(child);
        }
        else if (type == "audio" && !url.empty()) {
            child->type = NodeType::RADIO_STREAM;
            child->url = url;
            child->children_loaded = true;
            parent->children.push_back(child);
        }
        else if (type == "link" || type == "search" ||
                 (!url.empty() && url.find("Browse.ashx") != std::string::npos)) {
            child->type = NodeType::FOLDER;
            child->url = url;
            parent->children.push_back(child);
        }
        else if (node->children != nullptr) {
            child->type = NodeType::FOLDER;
            if (!key.empty()) {
                child->title = child->title.empty() ? key : child->title;
            }
            parent->children.push_back(child);
            parse_outline(node->children, child, false);
            child->children_loaded = true;
        }
        else if (!url.empty()) {
            child->type = NodeType::RADIO_STREAM;
            child->url = url;
            child->children_loaded = true;
            parent->children.push_back(child);
        }
    }
}

void OPMLParser::extract_feeds(const std::string& xml, std::vector<TreeNodePtr>& feeds) {
    // Reset XML error state
    reset_xml_error_state();

    // Suppress XML errors/warnings from outputting to terminal
    xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.size(), "opml", NULL, XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
    if (!doc) {
        std::string last_err = get_last_xml_error();
        if (!last_err.empty()) {
            LOG(fmt::format("[OPML] Extract feeds failed: {}", last_err));
        }
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return; }

    std::function<void(xmlNodePtr)> parse_outlines = [&](xmlNodePtr node) {
        for (; node; node = node->next) {
            if (node->type != XML_ELEMENT_NODE) continue;

            if (xmlStrcmp(node->name, (const xmlChar*)"outline") == 0) {
                std::string type = get_xml_prop_any(node, {"type"});
                std::string url = get_xml_prop_any(node, {"xmlUrl", "URL", "url"});
                std::string title = get_xml_prop_any(node, {"text", "title"});

                // RSS subscription type
                if ((type == "rss" || !url.empty()) && url.find("http") == 0) {
                    auto feed = std::make_shared<TreeNode>();
                    feed->title = title.empty() ? "Imported Feed" : title;
                    feed->url = url;
                    feed->type = NodeType::PODCAST_FEED;
                    feeds.push_back(feed);
                }

                // Recursively process child nodes
                if (node->children) {
                    parse_outlines(node->children);
                }
            } else if (node->children) {
                parse_outlines(node->children);
            }
        }
    };

    parse_outlines(root);
    xmlFreeDoc(doc);
}

// =========================================================
// RSS Parser implementation
// =========================================================
TreeNodePtr RSSParser::parse(std::string xml, const std::string& feed_url) {
    // Reset XML error state
    reset_xml_error_state();

    size_t p = xml.find('<');
    if (p != std::string::npos && p > 0) xml = xml.substr(p);
    if (xml.compare(0, 3, "\xEF\xBB\xBF") == 0) xml = xml.substr(3);

    // Suppress XML errors/warnings to prevent terminal screen corruption
    xmlDocPtr doc = xmlReadMemory(xml.c_str(), xml.size(), "feed", NULL, XML_PARSE_NOWARNING | XML_PARSE_NOERROR);
    if (!doc) {
        std::string last_err = get_last_xml_error();
        if (!last_err.empty()) {
            EVENT_LOG(fmt::format("XML parse failed: {}", last_err.substr(0, 50)));
        } else {
            EVENT_LOG("XML parse failed: invalid document");
        }
        return nullptr;
    }

    auto channel = std::make_shared<TreeNode>();
    channel->type = NodeType::PODCAST_FEED;
    channel->children_loaded = true;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return nullptr; }

    bool is_youtube_rss = (feed_url.find("youtube.com/feeds/videos.xml") != std::string::npos);

    if (xmlStrcmp(root->name, (const xmlChar*)"rss") == 0 || xmlStrcmp(root->name, (const xmlChar*)"rdf:RDF") == 0) {
        for (xmlNodePtr n = root->children; n; n = n->next) {
            if (xmlStrcmp(n->name, (const xmlChar*)"channel") == 0) {
                parse_channel(n, channel);
            }
        }
    } else if (xmlStrcmp(root->name, (const xmlChar*)"feed") == 0) {
        if (is_youtube_rss) {
            parse_youtube_atom(doc, channel);
        } else {
            parse_atom_feed(root, channel);
        }
    }

    xmlFreeDoc(doc);
    return channel;
}

void RSSParser::parse_channel(xmlNodePtr ch, TreeNodePtr c) {
    for (xmlNodePtr p = ch->children; p; p = p->next) {
        const char* local_name = (const char*)p->name;

        // RSS 2.0 standard field - title
        if (strcmp(local_name, "title") == 0) {
            xmlChar* t = xmlNodeGetContent(p);
            if (t) { c->title = (char*)t; xmlFree(t); }
        }
        // RSS 2.0 standard field - description (podcast summary)
        else if (strcmp(local_name, "description") == 0) {
            // Can store podcast summary, currently ignored
        }
        // RSS 2.0 standard field - item (episode)
        else if (strcmp(local_name, "item") == 0) {
            auto e = parse_item(p);
            if (e) { e->parent = c; c->children.push_back(e); }
        }
        // iTunes extension - author
        else if (strcmp(local_name, "itunes:author") == 0) {
            xmlChar* t = xmlNodeGetContent(p);
            if (t) {
                // Can store author info, currently ignored
                xmlFree(t);
            }
        }
        // iTunes extension - image (podcast cover)
        else if (strcmp(local_name, "itunes:image") == 0) {
            xmlChar* href = xmlGetProp(p, (const xmlChar*)"href");
            if (href) {
                // Can store cover URL, currently ignored
                xmlFree(href);
            }
        }
        // iTunes extension - category
        else if (strcmp(local_name, "itunes:category") == 0) {
            xmlChar* txt = xmlGetProp(p, (const xmlChar*)"text");
            if (txt) {
                // Can store category info, currently ignored
                xmlFree(txt);
            }
        }
        // Atom extension - link (pagination/self-reference)
        // Namespace: xmlns:atom="http://www.w3.org/2005/Atom"
        else if (strcmp(local_name, "link") == 0 ||
                 strcmp(local_name, "atom:link") == 0) {
            xmlChar* rel = xmlGetProp(p, (const xmlChar*)"rel");
            xmlChar* href = xmlGetProp(p, (const xmlChar*)"href");
            if (rel && href) {
                std::string rel_val = (char*)rel;
                std::string href_val = (char*)href;
                // atom:link rel="next" - pagination support
                if (rel_val == "next" && !href_val.empty()) {
                    LOG(fmt::format("[RSS] Found next page: {}", href_val));
                }
                xmlFree(rel);
                xmlFree(href);
            } else {
                if (rel) xmlFree(rel);
                if (href) xmlFree(href);
            }
        }
    }
}

TreeNodePtr RSSParser::parse_item(xmlNodePtr it) {
    auto ep = std::make_shared<TreeNode>();
    ep->type = NodeType::PODCAST_EPISODE;
    std::string url;        // Audio/video URL
    std::string media_url;  // Media RSS URL (fallback)

    for (xmlNodePtr i = it->children; i; i = i->next) {
        const char* local_name = (const char*)i->name;

        // RSS 2.0 standard field - title
        if (strcmp(local_name, "title") == 0) {
            xmlChar* t = xmlNodeGetContent(i);
            if (t) { ep->title = (char*)t; xmlFree(t); }
        }
        // RSS 2.0 enclosure - highest priority
        else if (strcmp(local_name, "enclosure") == 0) {
            std::string u = get_xml_prop_any(i, {"url"});
            if (!u.empty()) url = u;
        }
        // Media RSS support (media:content) - fallback for enclosure
        // Namespace: xmlns:media="http://search.yahoo.com/mrss/"
        else if (strcmp(local_name, "content") == 0 ||
                 strcmp(local_name, "media:content") == 0) {
            xmlChar* media_url_attr = xmlGetProp(i, (const xmlChar*)"url");
            xmlChar* type_attr = xmlGetProp(i, (const xmlChar*)"type");
            if (media_url_attr) {
                std::string murl = (char*)media_url_attr;
                std::string mime = type_attr ? (char*)type_attr : "";
                // Only accept audio/video type media:content
                if (mime.find("audio") != std::string::npos ||
                    mime.find("video") != std::string::npos ||
                    murl.find(".mp3") != std::string::npos ||
                    murl.find(".mp4") != std::string::npos ||
                    murl.find(".m4a") != std::string::npos ||
                    murl.find(".ogg") != std::string::npos ||
                    murl.find(".webm") != std::string::npos) {
                    if (media_url.empty()) media_url = murl;
                }
                xmlFree(media_url_attr);
                if (type_attr) xmlFree(type_attr);
            }
        }
        // Media RSS group - nested media:content
        else if (strcmp(local_name, "group") == 0 ||
                 strcmp(local_name, "media:group") == 0) {
            for (xmlNodePtr mc = i->children; mc; mc = mc->next) {
                const char* mc_name = (const char*)mc->name;
                if (strcmp(mc_name, "content") == 0 ||
                    strcmp(mc_name, "media:content") == 0) {
                    xmlChar* media_url_attr = xmlGetProp(mc, (const xmlChar*)"url");
                    xmlChar* type_attr = xmlGetProp(mc, (const xmlChar*)"type");
                    if (media_url_attr) {
                        std::string murl = (char*)media_url_attr;
                        std::string mime = type_attr ? (char*)type_attr : "";
                        if ((mime.find("audio") != std::string::npos ||
                             mime.find("video") != std::string::npos ||
                             murl.find(".mp3") != std::string::npos ||
                             murl.find(".m4a") != std::string::npos) &&
                            media_url.empty()) {
                            media_url = murl;
                        }
                        xmlFree(media_url_attr);
                        if (type_attr) xmlFree(type_attr);
                    }
                }
            }
        }
        // link field - last fallback
        else if (strcmp(local_name, "link") == 0 && url.empty() && media_url.empty()) {
            xmlChar* t = xmlNodeGetContent(i);
            if (t) {
                std::string l = (char*)t;
                xmlFree(t);
                if (l.find(".mp3") != std::string::npos ||
                    l.find(".mp4") != std::string::npos ||
                    l.find(".m4a") != std::string::npos) url = l;
            }
        }
        // pubDate / dc:date - publication date
        else if (strcmp(local_name, "pubDate") == 0 ||
                 strcmp(local_name, "dc:date") == 0) {
            xmlChar* t = xmlNodeGetContent(i);
            if (t) { ep->subtext = (char*)t; xmlFree(t); }
        }
        // iTunes extension - duration
        else if (strcmp(local_name, "duration") == 0 ||
                 strcmp(local_name, "itunes:duration") == 0) {
            xmlChar* t = xmlNodeGetContent(i);
            if (t) {
                std::string dur = (char*)t;
                xmlFree(t);
                int d = 0;
                size_t colon1 = dur.find(':');
                if (colon1 != std::string::npos) {
                    size_t colon2 = dur.find(':', colon1 + 1);
                    if (colon2 != std::string::npos) {
                        d = std::stoi(dur.substr(0, colon1)) * 3600 +
                            std::stoi(dur.substr(colon1 + 1, colon2 - colon1 - 1)) * 60 +
                            std::stoi(dur.substr(colon2 + 1));
                    } else {
                        d = std::stoi(dur.substr(0, colon1)) * 60 +
                            std::stoi(dur.substr(colon1 + 1));
                    }
                } else {
                    try { d = std::stoi(dur); } catch (...) {}
                }
                ep->duration = d;
            }
        }
        // iTunes extension - author (stored in extra field, not used yet)
        else if (strcmp(local_name, "author") == 0 ||
                 strcmp(local_name, "itunes:author") == 0) {
            // Can store author info, currently ignored
        }
        // iTunes extension - image (cover art)
        else if (strcmp(local_name, "itunes:image") == 0) {
            xmlChar* href = xmlGetProp(i, (const xmlChar*)"href");
            if (href) {
                // Can store cover URL, currently ignored
                xmlFree(href);
            }
        }
        // Podcast 2.0 extension - chapters
        else if (strcmp(local_name, "chapters") == 0 ||
                 strcmp(local_name, "podcast:chapters") == 0) {
            xmlChar* ch_url = xmlGetProp(i, (const xmlChar*)"url");
            if (ch_url) {
                // Can store chapters URL, currently ignored
                xmlFree(ch_url);
            }
        }
        // Podcast 2.0 extension - transcript
        else if (strcmp(local_name, "transcript") == 0 ||
                 strcmp(local_name, "podcast:transcript") == 0) {
            xmlChar* tr_url = xmlGetProp(i, (const xmlChar*)"url");
            if (tr_url) {
                // Can store transcript URL, currently ignored
                xmlFree(tr_url);
            }
        }
    }

    // URL priority strategy: enclosure > media:content > link
    std::string final_url = url.empty() ? media_url : url;

    if (!final_url.empty()) {
        ep->url = final_url;
        ep->children_loaded = true;
        // Detect video URL
        URLType url_type = URLClassifier::classify(final_url);
        if (url_type == URLType::VIDEO_FILE) {
            ep->is_youtube = false; // Not YouTube but is video
        }
        return ep;
    }
    return nullptr;
}

void RSSParser::parse_atom_feed(xmlNodePtr feed, TreeNodePtr c) {
    for (xmlNodePtr n = feed->children; n; n = n->next) {
        if (xmlStrcmp(n->name, (const xmlChar*)"title") == 0) {
            xmlChar* t = xmlNodeGetContent(n);
            if (t) { c->title = (char*)t; xmlFree(t); }
        } else if (xmlStrcmp(n->name, (const xmlChar*)"entry") == 0) {
            auto e = parse_atom_entry(n);
            if (e) { e->parent = c; c->children.push_back(e); }
        }
    }
}

TreeNodePtr RSSParser::parse_atom_entry(xmlNodePtr entry) {
    auto ep = std::make_shared<TreeNode>();
    ep->type = NodeType::PODCAST_EPISODE;

    for (xmlNodePtr child = entry->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) continue;

        const char* name = (const char*)child->name;
        if (strcmp(name, "title") == 0) {
            xmlChar* t = xmlNodeGetContent(child);
            if (t) { ep->title = (char*)t; xmlFree(t); }
        } else if (strcmp(name, "link") == 0) {
            xmlChar* href = xmlGetProp(child, (const xmlChar*)"href");
            xmlChar* type = xmlGetProp(child, (const xmlChar*)"type");
            if (href) {
                std::string link = (char*)href;
                std::string mime = type ? (char*)type : "";
                if (mime.find("audio") != std::string::npos ||
                    mime.find("video") != std::string::npos ||
                    link.find(".mp3") != std::string::npos ||
                    link.find(".mp4") != std::string::npos ||
                    link.find(".m4a") != std::string::npos) {
                    ep->url = link;
                }
                xmlFree(href);
                if (type) xmlFree(type);
            }
        } else if (strcmp(name, "published") == 0 || strcmp(name, "updated") == 0) {
            xmlChar* t = xmlNodeGetContent(child);
            if (t) { ep->subtext = (char*)t; xmlFree(t); }
        }
    }

    if (!ep->url.empty()) return ep;
    return nullptr;
}

void RSSParser::parse_youtube_atom(xmlDocPtr doc, TreeNodePtr c) {
    xmlXPathContextPtr xpath_ctx = xmlXPathNewContext(doc);
    if (!xpath_ctx) return;

    xmlXPathRegisterNs(xpath_ctx, (const xmlChar*)"atom", (const xmlChar*)"http://www.w3.org/2005/Atom");
    xmlXPathRegisterNs(xpath_ctx, (const xmlChar*)"yt", (const xmlChar*)"http://www.youtube.com/xml/schemas/2015");
    xmlXPathRegisterNs(xpath_ctx, (const xmlChar*)"media", (const xmlChar*)"http://search.yahoo.com/mrss/");

    xmlXPathObjectPtr title_result = xmlXPathNodeEval(
        xmlDocGetRootElement(doc),
        (const xmlChar*)".//*[local-name()='title'][1]",
        xpath_ctx);

    if (title_result && title_result->nodesetval && title_result->nodesetval->nodeNr > 0) {
        xmlChar* t = xmlNodeGetContent(title_result->nodesetval->nodeTab[0]);
        if (t) { c->title = (char*)t; xmlFree(t); }
    }
    if (title_result) xmlXPathFreeObject(title_result);

    xmlXPathObjectPtr entries = xmlXPathNodeEval(
        xmlDocGetRootElement(doc),
        (const xmlChar*)".//*[local-name()='entry']",
        xpath_ctx);

    if (entries && entries->nodesetval) {
        for (int i = 0; i < entries->nodesetval->nodeNr; ++i) {
            auto ep = parse_youtube_entry(entries->nodesetval->nodeTab[i]);
            if (ep) { ep->parent = c; c->children.push_back(ep); }
        }
    }

    if (entries) xmlXPathFreeObject(entries);
    xmlXPathFreeContext(xpath_ctx);
}

TreeNodePtr RSSParser::parse_youtube_entry(xmlNodePtr entry) {
    auto ep = std::make_shared<TreeNode>();
    ep->type = NodeType::PODCAST_EPISODE;
    ep->is_youtube = true;
    std::string video_id, title, link_url;

    for (xmlNodePtr child = entry->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) continue;
        const char* local_name = (const char*)child->name;

        if (strcmp(local_name, "title") == 0 && title.empty()) {
            xmlChar* t = xmlNodeGetContent(child);
            if (t) { title = (char*)t; xmlFree(t); }
        } else if (strcmp(local_name, "videoId") == 0) {
            xmlChar* vid = xmlNodeGetContent(child);
            if (vid) { std::string vid_str = (char*)vid; if (vid_str.length() == 11) video_id = vid_str; xmlFree(vid); }
        } else if (strcmp(local_name, "link") == 0) {
            xmlChar* href = xmlGetProp(child, (const xmlChar*)"href");
            xmlChar* rel = xmlGetProp(child, (const xmlChar*)"rel");
            if (href && (!rel || strcmp((char*)rel, "alternate") == 0)) link_url = (char*)href;
            if (href) xmlFree(href);
            if (rel) xmlFree(rel);
        }
    }

    if (!video_id.empty()) ep->url = fmt::format("https://www.youtube.com/watch?v={}", video_id);
    else if (!link_url.empty()) ep->url = link_url;
    ep->title = title;

    if (!ep->title.empty() && !ep->url.empty()) return ep;
    return nullptr;
}

// =========================================================
// YouTube Channel Parser implementation
// =========================================================
TreeNodePtr YouTubeChannelParser::parse(const std::string& channel_url) {
    auto channel_node = std::make_shared<TreeNode>();
    channel_node->type = NodeType::PODCAST_FEED;
    channel_node->children_loaded = false;
    channel_node->is_youtube = true;
    channel_node->title = URLClassifier::extract_channel_name(channel_url);
    channel_node->channel_name = channel_node->title;

    EVENT_LOG(fmt::format("YouTube Channel: {}", channel_url));
    LOG(fmt::format("[YouTube] Parsing: {}", channel_url));

    // Use simplest command (reference A version successful experience)
    // --flat-playlist: quickly list, no download
    // --dump-json: output JSON format
    // --no-warnings: suppress warning output
    // 2>&1: redirect stderr to stdout, capture all output uniformly
    // This avoids any terminal output causing screen corruption
    std::string cmd = fmt::format("yt-dlp --flat-playlist --dump-json --no-warnings \"{}\" 2>&1", channel_url);
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        channel_node->parse_failed = true;
        channel_node->error_msg = "Failed to execute yt-dlp";
        EVENT_LOG("YouTube: Failed to execute yt-dlp");
        return channel_node;
    }

    char buf[8192];
    int count = 0;
    std::vector<YouTubeVideoInfo> videos;

    while (fgets(buf, sizeof(buf), fp)) {
        try {
            std::string line = buf;
            // Remove newline characters
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            // Skip empty lines and non-JSON lines
            if (line.empty() || line[0] != '{') continue;

            auto j = json::parse(line);
            std::string id = j.value("id", "");
            std::string title = j.value("title", "Untitled");

            if (!id.empty()) {
                auto ep = std::make_shared<TreeNode>();
                ep->type = NodeType::PODCAST_EPISODE;
                ep->title = title;
                ep->url = fmt::format("https://www.youtube.com/watch?v={}", id);
                ep->is_youtube = true;
                ep->children_loaded = true;
                ep->parent = channel_node;
                channel_node->children.push_back(ep);
                videos.push_back({id, title, ep->url});
                count++;
            }
        } catch (...) {
            // Skip line on parse error
        }
    }

    pclose(fp);

    if (count > 0) {
        channel_node->children_loaded = true;
        YouTubeCache::instance().update(channel_url, channel_node->title, videos);
        EVENT_LOG(fmt::format("Found {} videos", count));
        LOG(fmt::format("[YouTube] Parsed {} videos", count));
    } else {
        channel_node->parse_failed = true;
        channel_node->error_msg = "No videos found";
        EVENT_LOG("YouTube: No videos found");
    }

    return channel_node;
}

} // namespace podradio
