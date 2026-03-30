#ifndef PODRADIO_URL_CLASSIFIER_H
#define PODRADIO_URL_CLASSIFIER_H

#include "common.h"

#include <string>

namespace podradio {

// =========================================================
// URL分类器
// =========================================================
class URLClassifier {
public:
    static inline URLType classify(const std::string& url) {
        if (url.empty()) return URLType::UNKNOWN;

        // 本地文件
        if (url.find("file://") == 0 || url[0] == '/') return URLType::RADIO_STREAM;

        // V2.39: 检测视频文件URL
        if (url.find(".mp4") != std::string::npos ||
            url.find(".webm") != std::string::npos ||
            url.find(".mkv") != std::string::npos ||
            url.find(".avi") != std::string::npos ||
            url.find(".mov") != std::string::npos) {
            return URLType::VIDEO_FILE;
        }

        if (url.find("youtube.com/@") != std::string::npos ||
            url.find("youtube.com/channel/") != std::string::npos ||
            url.find("youtube.com/c/") != std::string::npos) return URLType::YOUTUBE_CHANNEL;
        if (url.find("youtube.com/feeds/videos.xml") != std::string::npos) return URLType::YOUTUBE_RSS;
        if (url.find("youtube.com/watch") != std::string::npos ||
            url.find("youtu.be/") != std::string::npos) return URLType::YOUTUBE_VIDEO;
        if (url.find("youtube.com/playlist") != std::string::npos) return URLType::YOUTUBE_PLAYLIST;
        if (url.find("podcasts.apple.com") != std::string::npos) return URLType::APPLE_PODCAST;
        //Tune.ashx 需要区分目录和流
        // c=pbrowse 表示浏览目录，应该是 OPML 类型
        if (url.find("Tune.ashx") != std::string::npos) {
            if (url.find("c=pbrowse") != std::string::npos ||
                url.find("c=sbrowse") != std::string::npos) {
                return URLType::OPML;  // 目录浏览
            }
            return URLType::RADIO_STREAM;  // 音频流
        }
        if (url.find(".m3u8") != std::string::npos || url.find(".mp3") != std::string::npos ||
            url.find(".aac") != std::string::npos) return URLType::RADIO_STREAM;
        if (url.find("Browse.ashx") != std::string::npos || url.find(".opml") != std::string::npos) return URLType::OPML;
        if (url.find(".xml") != std::string::npos || url.find("/feed") != std::string::npos ||
            url.find("/rss") != std::string::npos) return URLType::RSS_PODCAST;
        return URLType::UNKNOWN;
    }

    static inline std::string type_name(URLType type) {
        switch (type) {
            case URLType::OPML: return "OPML";
            case URLType::RSS_PODCAST: return "RSS";
            case URLType::YOUTUBE_RSS: return "YouTube RSS";
            case URLType::YOUTUBE_CHANNEL: return "YouTube Channel";
            case URLType::YOUTUBE_VIDEO: return "YouTube Video";
            case URLType::APPLE_PODCAST: return "Apple Podcast";
            case URLType::RADIO_STREAM: return "Radio Stream";
            case URLType::VIDEO_FILE: return "Video File";
            default: return "Unknown";
        }
    }

    static inline bool is_youtube(URLType type) {
        return type == URLType::YOUTUBE_RSS || type == URLType::YOUTUBE_CHANNEL ||
               type == URLType::YOUTUBE_VIDEO || type == URLType::YOUTUBE_PLAYLIST;
    }

    static inline bool is_video(URLType type) {
        return is_youtube(type) || type == URLType::VIDEO_FILE;
    }

    static inline std::string extract_channel_name(const std::string& url) {
        size_t pos;
        if ((pos = url.find("/@")) != std::string::npos) {
            std::string name = url.substr(pos + 2);
            size_t end = name.find_first_of("/?#");
            if (end != std::string::npos) name = name.substr(0, end);
            return name.empty() ? "YouTube Channel" : name;
        }
        if ((pos = url.find("/channel/")) != std::string::npos) {
            std::string name = url.substr(pos + 9);
            size_t end = name.find_first_of("/?#");
            if (end != std::string::npos) name = name.substr(0, end);
            return name.empty() ? "YouTube Channel" : "ch:" + name.substr(0, std::min((size_t)8, name.length()));
        }
        if ((pos = url.find("/c/")) != std::string::npos) {
            std::string name = url.substr(pos + 3);
            size_t end = name.find_first_of("/?#");
            if (end != std::string::npos) name = name.substr(0, end);
            return name.empty() ? "YouTube Channel" : name;
        }
        return "YouTube Channel";
    }
};

} // namespace podradio

#endif // PODRADIO_URL_CLASSIFIER_H
