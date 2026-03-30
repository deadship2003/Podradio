/*
 * network.cpp - Network implementation (libcurl)
 *
 * HTTP GET wrapper with configurable SSL verification.
 *
 * Original behaviour (ssl_verify=false) disables certificate checks
 * for maximum compatibility with self-signed / misconfigured servers.
 * When ssl_verify=true, full peer and host verification is enabled.
 */

#include "podradio/network.h"
#include "podradio/logger.h"
#include "podradio/event_log.h"

#include <regex>

// LOG / EVENT_LOG fallback macros (resolved from Logger/EventLog at link time)
#ifndef LOG
#define LOG(msg) (void)(msg)
#endif
#ifndef EVENT_LOG
#define EVENT_LOG(msg) (void)(msg)
#endif

namespace podradio
{

// ====================================================================
// Write callback for libcurl
// ====================================================================

size_t Network::write_cb(void* ptr, size_t size, size_t nmemb, void* data)
{
    static_cast<std::string*>(data)->append(
        static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

// ====================================================================
// fetch - main HTTP GET method
// ====================================================================

std::string Network::fetch(const std::string& url, int timeout, bool ssl_verify)
{
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string data;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers,
        "Accept: application/xml,application/xhtml+xml,text/xml;q=0.9,*/*;q=0.8");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout));

    // SSL verification settings
    if (ssl_verify) {
        // Secure mode: enable full certificate and hostname verification
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        // Compatibility mode (original behaviour): skip all SSL checks
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
        curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);
#if LIBCURL_VERSION_NUM < 0x075600
        // CURLOPT_SSL_ENABLE_NPN deprecated since curl 7.86.0
        curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_NPN, 0L);
#endif
    }

    // Connection timeout and DNS caching
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        EVENT_LOG(fmt::format("Network error: {} (URL: {})",
                              curl_easy_strerror(res),
                              url.substr(0, 60)));
    }
    return data;
}

// ====================================================================
// fetch_secure - convenience alias with SSL verification enabled
// ====================================================================

std::string Network::fetch_secure(const std::string& url, int timeout)
{
    return fetch(url, timeout, /*ssl_verify=*/true);
}

// ====================================================================
// lookup_apple_feed - resolve Apple Podcasts URL to RSS feed
// ====================================================================

std::string Network::lookup_apple_feed(const std::string& url)
{
    std::regex id_regex("/id(\\d+)");
    std::smatch match;
    if (!std::regex_search(url, match, id_regex)) return "";

    std::string response = fetch(
        "https://itunes.apple.com/lookup?id=" + match[1].str() + "&entity=podcast");
    if (response.empty()) return "";

    try {
        json j = json::parse(response);
        if (j.contains("results") && !j["results"].empty()) {
            return j["results"][0].value("feedUrl", "");
        }
    } catch (...) {}

    return "";
}

} // namespace podradio
