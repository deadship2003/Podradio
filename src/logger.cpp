#include "podradio/logger.h"

namespace podradio
{

    Logger& Logger::instance() {
        static Logger l;
        return l;
    }

    void Logger::init() {
        const char* home = std::getenv("HOME");
        if (home) {
            std::string dir = std::string(home) + DATA_DIR;
            fs::create_directories(dir);
            file_.open(dir + "/podradio.log", std::ios::app);
            if (file_.is_open()) {
                file_ << "========================================\n";
                file_ << fmt::format("{} {} by {}\n", APP_NAME, VERSION, AUTHOR);
                file_ << "========================================\n";
                file_.flush();
            }
        }
    }

    void Logger::log(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (file_.is_open()) {
            // All log entries get a timestamp
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
            file_ << fmt::format("[{}.{:03d}] {}", buf, ms.count(), msg) << std::endl;
            file_.flush();
        }
    }

    Logger::~Logger() {
        if (file_.is_open()) {
            file_ << "========================================\n";
            file_.close();
        }
    }

} // namespace podradio
