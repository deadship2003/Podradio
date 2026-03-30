#pragma once

/*
 * cache_manager.h - In-memory cache manager with DB persistence
 *
 * Tracks cached/downloaded URLs and local file paths.
 * In-memory sets provide fast O(1) lookups at runtime.
 * All state is persisted to the url_cache table in SQLite via
 * DatabaseManager (no separate JSON file).
 *
 * Singleton pattern. Thread-safe through DatabaseManager's internal mutex.
 */

#include "common.h"

#include <set>
#include <map>
#include <string>

namespace podradio
{

class CacheManager
{
public:
    static CacheManager& instance() { static CacheManager cm; return cm; }

    /// Load cache state from the url_cache table into memory.
    /// Call once at startup after DatabaseManager::init().
    void load();

    /// Mark a URL as stream-cached (persisted to DB immediately).
    void mark_cached(const std::string& url);

    /// Mark a URL as downloaded with an optional local file path.
    void mark_downloaded(const std::string& url, const std::string& local_file = "");

    /// Check if a URL is stream-cached (in-memory + DB fallback).
    bool is_cached(const std::string& url) const;

    /// Check if a URL is downloaded (in-memory + DB fallback).
    bool is_downloaded(const std::string& url) const;

    /// Get the local file path for a downloaded URL.
    std::string get_local_file(const std::string& url) const;

    /// Set local file path for a downloaded URL (also marks as downloaded).
    void set_local_file(const std::string& url, const std::string& path);

    /// Composite check: is the URL either cached or downloaded?
    bool is_cached_or_downloaded(const std::string& url) const;

    /// Clear the cached flag for a URL.
    void clear_cache(const std::string& url);

    /// Clear the downloaded flag (and local file record) for a URL.
    /// Does NOT delete the actual local file.
    void clear_download(const std::string& url);

private:
    CacheManager() = default;

    std::set<std::string> cached_;
    std::set<std::string> downloaded_;
    std::map<std::string, std::string> local_files_;
};

} // namespace podradio
