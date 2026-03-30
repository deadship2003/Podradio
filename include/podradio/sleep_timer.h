#pragma once

#include "common.h"

namespace podradio {

// =========================================================
// Sleep Timer - Countdown timer for auto-pause
// Singleton pattern. Supports multiple time format inputs.
// =========================================================
class SleepTimer {
public:
    static SleepTimer& instance();

    // Set timer duration in seconds
    void set_duration(int seconds);

    // Set timer with string input (supports "5h", "30m", "1:25:15", "90")
    void set_duration(const std::string& time_str);

    // Check if timer has expired (auto-deactivates)
    bool check_expired();

    // Get remaining seconds
    int remaining_seconds();

    // Check if timer is active
    bool is_active() const;

    // Cancel timer
    void cancel();

    // Parse time string to seconds (static, for external use)
    // Supports: "5h", "30m", "90s", "1:25:15", pure number
    // Pure number logic: val < 100 -> hours, val >= 100 -> minutes
    static int parse_time_string(const std::string& time_str);

private:
    SleepTimer();

    int duration_seconds_;
    std::chrono::steady_clock::time_point start_time_;
    bool active_;
};

} // namespace podradio
