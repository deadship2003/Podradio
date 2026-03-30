#include "podradio/sleep_timer.h"
#include "podradio/event_log.h"

namespace podradio
{

SleepTimer& SleepTimer::instance() {
    static SleepTimer st;
    return st;
}

SleepTimer::SleepTimer() : duration_seconds_(0), active_(false) {}

void SleepTimer::set_duration(int seconds) {
    duration_seconds_ = seconds;
    start_time_ = std::chrono::steady_clock::now();
    active_ = true;
    EVENT_LOG(fmt::format("Sleep timer set: {}s", seconds));
}

void SleepTimer::set_duration(const std::string& time_str) {
    int seconds = parse_time_string(time_str);
    if (seconds > 0) set_duration(seconds);
}

bool SleepTimer::check_expired() {
    if (!active_ || duration_seconds_ <= 0) return false;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    if (elapsed >= duration_seconds_) {
        active_ = false;
        EVENT_LOG("Sleep timer expired!");
        return true;
    }
    return false;
}

int SleepTimer::remaining_seconds() {
    if (!active_ || duration_seconds_ <= 0) return 0;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    int remaining = duration_seconds_ - elapsed;
    return remaining > 0 ? remaining : 0;
}

bool SleepTimer::is_active() const {
    return active_;
}

void SleepTimer::cancel() {
    active_ = false;
    EVENT_LOG("Sleep timer cancelled");
}

int SleepTimer::parse_time_string(const std::string& time_str) {
    int seconds = 0;
    std::string s = time_str;

    // Remove possible negative prefix (e.g. -5h)
    if (!s.empty() && s[0] == '-') {
        s = s.substr(1);
    }

    // Check suffix format: 5h, 30m, 90s
    if (s.size() >= 2) {
        char suffix = std::tolower(s.back());
        std::string num_part = s.substr(0, s.size() - 1);
        try {
            int val = std::stoi(num_part);
            if (suffix == 'h') return val * 3600;
            if (suffix == 'm') return val * 60;
            if (suffix == 's') return val;
        } catch (...) {}
    }

    // Check HH:MM:SS format
    if (s.find(':') != std::string::npos) {
        std::vector<int> parts;
        std::stringstream ss(s);
        std::string part;
        while (std::getline(ss, part, ':')) {
            try { parts.push_back(std::stoi(part)); }
            catch (...) { parts.push_back(0); }
        }
        if (parts.size() >= 3) seconds = parts[0] * 3600 + parts[1] * 60 + parts[2];
        else if (parts.size() == 2) seconds = parts[0] * 60 + parts[1];
        else if (parts.size() == 1) seconds = parts[0];
        return seconds;
    }

    // Pure number auto-detect logic
    // Rule: val < 100 -> hours, val >= 100 -> minutes
    try {
        int val = std::stoi(s);
        if (val >= 100) {
            seconds = val * 60;   // >=100 treated as minutes
        } else {
            seconds = val * 3600; // <100 treated as hours
        }
    } catch (...) {}

    return seconds;
}

} // namespace podradio
