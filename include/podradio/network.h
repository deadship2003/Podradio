#pragma once

/*
 * network.h - HTTP fetch layer (libcurl wrapper)
 *
 * Provides synchronous HTTP GET with optional SSL verification control.
 *
 * SECURITY FIX: fetch() now accepts a bool ssl_verify parameter.
 *   - false (default): disables SSL verification (backward compatible behaviour)
 *   - true:  enables SSL peer + host verification
 *
 * fetch_secure() is a convenience alias that always enables verification.
 */

#include "common.h"

#include <string>

namespace podradio
{

class Network
{
public:
    /// Fetch URL content via HTTP GET.
    /// @param url         Remote URL to fetch
    /// @param timeout     Connection/request timeout in seconds
    /// @param ssl_verify  When false, skip certificate verification (default for compat)
    ///                    When true, enable full SSL peer+host verification
    /// @return Response body, or empty string on error
    static std::string fetch(const std::string& url,
                             int timeout = DEFAULT_NETWORK_TIMEOUT_SEC,
                             bool ssl_verify = false);

    /// Convenience: fetch with SSL verification always enabled.
    static std::string fetch_secure(const std::string& url,
                                    int timeout = DEFAULT_NETWORK_TIMEOUT_SEC);

    /// Resolve an Apple Podcasts lookup URL to its RSS feed URL.
    static std::string lookup_apple_feed(const std::string& url);

private:
    static size_t write_cb(void* ptr, size_t size, size_t nmemb, void* data);
};

} // namespace podradio
