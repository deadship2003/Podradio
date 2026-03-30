#pragma once

#include "common.h"

namespace podradio
{

    // =========================================================
    // EventLog singleton - in-memory event log with timestamps
    //
    // Stores up to 1024 timestamped log entries in a deque.
    // Each push() also writes to the file log via LOG().
    // =========================================================
    class EventLog {
    public:
        static EventLog& instance();

        // Push a message to the event log (auto-timestamped, also writes to LOG)
        void push(const std::string& msg);

        // Get all log entries (thread-safe copy)
        std::deque<LogEntry> get();

        // Get number of log entries
        size_t size();

    private:
        EventLog() {}
        EventLog(const EventLog&) = delete;
        EventLog& operator=(const EventLog&) = delete;

        std::deque<LogEntry> logs_;
        std::mutex mtx_;
    };

} // namespace podradio

// EVENT_LOG macro - push a message to the event log and file log
#define EVENT_LOG(msg) ::podradio::EventLog::instance().push(msg)
