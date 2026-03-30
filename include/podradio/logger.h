#pragma once

#include "common.h"

namespace podradio
{

    // =========================================================
    // Logger singleton - writes timestamped entries to podradio.log
    // =========================================================
    class Logger {
    public:
        static Logger& instance();

        // Initialize log file (creates data directory, opens log file, writes header)
        void init();

        // Write a timestamped log entry
        void log(const std::string& msg);

        ~Logger();

    private:
        Logger() = default;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        std::ofstream file_;
        std::mutex mtx_;
    };

} // namespace podradio

// LOG macro - shorthand for writing to the log file
#define LOG(msg) ::podradio::Logger::instance().log(msg)
