/*
 * cache_manager.cpp - In-memory cache manager with DB persistence
 *
 * Previously persisted to ~/.local/share/podradio/cached_urls.json.
 * Now all persistence goes through DatabaseManager::url_cache_* methods,
 * eliminating the redundant JSON file.
 *
 * Migration: If cached_urls.json exists, it will be ignored.
 * The url_cache SQLite table is the single source of truth.
 */

#include "podradio/cache_manager.h"
#include "podradio/database.h"

namespace podradio
{

// ====================================================================
// Load - populate in-memory sets from the url_cache table
// ====================================================================

void CacheManager::load()
{
    cached_.clear();
    downloaded_.clear();
    local_files_.clear();

    DatabaseManager::instance().url_cache_load_all(cached_, downloaded_, local_files_);
}

// ====================================================================
// Cache marking
// ====================================================================

void CacheManager::mark_cached(const std::string& url)
{
    cached_.insert(url);
    DatabaseManager::instance().url_cache_mark_cached(url);
}

bool CacheManager::is_cached(const std::string& url) const
{
    // Fast in-memory check first
    if (cached_.count(url)) return true;
    // Fallback to DB (handles entries added before in-memory load)
    return DatabaseManager::instance().url_cache_is_cached(url);
}

// ====================================================================
// Download marking
// ====================================================================

void CacheManager::mark_downloaded(const std::string& url, const std::string& local_file)
{
    downloaded_.insert(url);
    if (!local_file.empty()) local_files_[url] = local_file;
    DatabaseManager::instance().url_cache_mark_downloaded(url, local_file);
}

bool CacheManager::is_downloaded(const std::string& url) const
{
    if (downloaded_.count(url)) return true;
    return DatabaseManager::instance().url_cache_is_downloaded(url);
}

// ====================================================================
// Local file mapping
// ====================================================================

std::string CacheManager::get_local_file(const std::string& url) const
{
    auto it = local_files_.find(url);
    if (it != local_files_.end()) return it->second;
    // Fallback to DB
    return DatabaseManager::instance().url_cache_get_local_file(url);
}

void CacheManager::set_local_file(const std::string& url, const std::string& path)
{
    local_files_[url] = path;
    mark_downloaded(url, path);
}

// ====================================================================
// Composite check
// ====================================================================

bool CacheManager::is_cached_or_downloaded(const std::string& url) const
{
    return is_cached(url) || is_downloaded(url);
}

// ====================================================================
// Clear individual entries
// ====================================================================

void CacheManager::clear_cache(const std::string& url)
{
    cached_.erase(url);
    DatabaseManager::instance().url_cache_clear_cached(url);
}

void CacheManager::clear_download(const std::string& url)
{
    downloaded_.erase(url);
    local_files_.erase(url);
    DatabaseManager::instance().url_cache_clear_downloaded(url);
}

} // namespace podradio
