#include "podradio/progress.h"

namespace podradio
{

    ProgressManager& ProgressManager::instance() {
        static ProgressManager pm;
        return pm;
    }

    std::string ProgressManager::start_download(const std::string& title, const std::string& url,
                                                  bool is_youtube) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string id = fmt::format("dl_{}", counter_++);
        downloads_[id] = {id, title, url, "Starting...", 0, "", 0, 0, 0, true, false, false, is_youtube, std::chrono::steady_clock::now()};
        return id;
    }

    // Enhanced update with ETA and byte statistics
    void ProgressManager::update(const std::string& id, int percent, const std::string& status,
                                 const std::string& speed, int eta_seconds,
                                 int64_t downloaded_bytes, int64_t total_bytes) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (downloads_.count(id)) {
            downloads_[id].percent = percent;
            downloads_[id].status = status;
            downloads_[id].speed = speed;
            downloads_[id].eta_seconds = eta_seconds;
            downloads_[id].downloaded_bytes = downloaded_bytes;
            downloads_[id].total_bytes = total_bytes;
        }
    }

    // Record timestamp on completion
    void ProgressManager::complete(const std::string& id, bool success) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (downloads_.count(id)) {
            downloads_[id].active = false;
            downloads_[id].complete = true;
            downloads_[id].failed = !success;
            downloads_[id].percent = 100;
            downloads_[id].status = success ? "Complete" : "Failed";
            downloads_[id].eta_seconds = 0;
            downloads_[id].complete_time = std::chrono::steady_clock::now();
            downloads_[id].speed = success ? "Done" : "Error";
        }
    }

    // Improved cleanup logic, completed downloads retained for a period before cleanup
    std::vector<DownloadProgress> ProgressManager::get_all() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<DownloadProgress> result;
        auto now = std::chrono::steady_clock::now();

        for (auto& [id, p] : downloads_) {
            result.push_back(p);
        }

        // Clean up completed downloads that have exceeded the display timeout
        for (auto it = downloads_.begin(); it != downloads_.end(); ) {
            if (it->second.complete && !it->second.active) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.complete_time).count();
                if (elapsed > DOWNLOAD_COMPLETE_DISPLAY_SEC) {
                    it = downloads_.erase(it);
                    continue;
                }
            }
            ++it;
        }
        return result;
    }

    // Get number of active downloads
    int ProgressManager::get_active_count() {
        std::lock_guard<std::mutex> lock(mtx_);
        int count = 0;
        for (const auto& [id, p] : downloads_) {
            if (p.active) count++;
        }
        return count;
    }

    // =========================================================
    // Download progress callback helpers
    // =========================================================

    // Format download speed
    std::string format_speed(double bytes_per_sec) {
        if (bytes_per_sec < 0 || std::isnan(bytes_per_sec) || std::isinf(bytes_per_sec)) {
            return "...";
        }
        if (bytes_per_sec < 1024) {
            return fmt::format("{:.0f}B/s", bytes_per_sec);
        } else if (bytes_per_sec < 1024 * 1024) {
            return fmt::format("{:.1f}KB/s", bytes_per_sec / 1024);
        } else if (bytes_per_sec < 1024 * 1024 * 1024) {
            return fmt::format("{:.1f}MB/s", bytes_per_sec / (1024 * 1024));
        } else {
            return fmt::format("{:.1f}GB/s", bytes_per_sec / (1024 * 1024 * 1024));
        }
    }

    // Format ETA time
    std::string format_eta(int seconds) {
        if (seconds <= 0 || seconds > 86400) return "--:--";
        int hours = seconds / 3600;
        int mins = (seconds % 3600) / 60;
        int secs = seconds % 60;
        if (hours > 0) {
            return fmt::format("{}:{:02d}:{:02d}", hours, mins, secs);
        } else {
            return fmt::format("{}:{:02d}", mins, secs);
        }
    }

    // libcurl progress callback function (CURLOPT_XFERINFOFUNCTION)
    int curl_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                               curl_off_t ultotal, curl_off_t ulnow) {
        (void)ultotal;  // Upload total - not handled in this callback
        (void)ulnow;    // Uploaded amount - not handled in this callback
        CurlProgressData* data = static_cast<CurlProgressData*>(clientp);
        if (!data || data->dl_id.empty()) return 0;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - data->last_time).count();

        // Update progress at most every 200ms to avoid excessive updates
        if (elapsed < 200 && dlnow < dltotal) return 0;

        int percent = 0;
        if (dltotal > 0) {
            percent = static_cast<int>((dlnow * 100) / dltotal);
            if (percent > 100) percent = 100;
        }

        // Calculate download speed
        double bytes_per_sec = 0;
        int eta_seconds = 0;
        if (elapsed > 0) {
            int64_t bytes_diff = dlnow - data->last_bytes;
            bytes_per_sec = (bytes_diff * 1000.0) / elapsed;

            // Calculate ETA
            if (bytes_per_sec > 0 && dltotal > dlnow) {
                int64_t remaining_bytes = dltotal - dlnow;
                eta_seconds = static_cast<int>(remaining_bytes / bytes_per_sec);
            }
        }

        // Update progress
        std::string speed_str = format_speed(bytes_per_sec);
        ProgressManager::instance().update(
            data->dl_id, percent,
            fmt::format("{}", format_eta(eta_seconds)),
            speed_str, eta_seconds,
            dlnow, dltotal
        );

        // Update last state
        data->last_bytes = dlnow;
        data->last_time = now;

        return 0;
    }

} // namespace podradio
