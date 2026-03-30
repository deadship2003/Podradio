#include "podradio/logger.h"
#include "podradio/event_log.h"

namespace podradio
{

    EventLog& EventLog::instance() {
        static EventLog el;
        return el;
    }

    void EventLog::push(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));

        LogEntry entry;
        entry.timestamp = fmt::format("[{}.{:03d}]", buf, ms.count());
        entry.message = msg;

        logs_.push_front(entry);
        while (logs_.size() > 1024) logs_.pop_back();  // Changed from 500 to 1024

        LOG(fmt::format("[{}.{:03d}] {}", buf, ms.count(), msg));
    }

    std::deque<LogEntry> EventLog::get() {
        std::lock_guard<std::mutex> lock(mtx_);
        return logs_;
    }

    size_t EventLog::size() {
        std::lock_guard<std::mutex> lock(mtx_);
        return logs_.size();
    }

} // namespace podradio
