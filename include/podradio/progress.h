#pragma once

#include "common.h"

namespace podradio
{

    // =========================================================
    // ProgressManager singleton - download progress tracking
    //
    // Manages active and completed download entries with
    // percentage, speed, ETA, and byte tracking. Completed
    // downloads are automatically cleaned up after a timeout.
    // =========================================================
    class ProgressManager {
    public:
        static ProgressManager& instance();

        // Start a new download, returns the download ID
        std::string start_download(const std::string& title, const std::string& url,
                                   bool is_youtube = false);

        // Enhanced update with ETA and byte statistics
        void update(const std::string& id, int percent, const std::string& status,
                    const std::string& speed = "", int eta_seconds = 0,
                    int64_t downloaded_bytes = 0, int64_t total_bytes = 0);

        // Mark download as complete with timestamp
        void complete(const std::string& id, bool success);

        // Get all downloads (also cleans up expired completed downloads)
        std::vector<DownloadProgress> get_all();

        // Get number of active downloads
        int get_active_count();

    private:
        ProgressManager() {}
        ProgressManager(const ProgressManager&) = delete;
        ProgressManager& operator=(const ProgressManager&) = delete;

        std::map<std::string, DownloadProgress> downloads_;
        std::mutex mtx_;
        int counter_ = 0;
    };

    // =========================================================
    // Download progress callback helpers
    // =========================================================

    // Format download speed with appropriate unit
    std::string format_speed(double bytes_per_sec);

    // Format ETA time as HH:MM:SS or MM:SS
    std::string format_eta(int seconds);

    // libcurl progress callback (CURLOPT_XFERINFOFUNCTION)
    int curl_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                               curl_off_t ultotal, curl_off_t ulnow);

} // namespace podradio
