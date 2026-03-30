/*
 * main.cpp - Entry point for PodRadio
 *
 * Handles command-line argument parsing and initializes
 * the application in either CLI or TUI mode.
 */

#include "podradio/app.h"
#include "podradio/logger.h"
#include "podradio/config.h"
#include "podradio/sleep_timer.h"
#include "podradio/common.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <getopt.h>
#include <chrono>
#include <thread>
#include <filesystem>

using namespace podradio;

// =========================================================
// print_usage - Display help text
// =========================================================
void print_usage() {
    std::cout << APP_NAME << " " << VERSION << " - Terminal Podcast/Radio Player\n";
    std::cout << "By " << AUTHOR << " <" << EMAIL << "> @" << BUILD_TIME << "\n\n";
    std::cout << "Usage:\n";
    std::cout << "  podradio                  Start the application (TUI mode)\n";
    std::cout << "  podradio -a <url>         Add feed from URL\n";
    std::cout << "  podradio -i <file>        Import OPML subscriptions\n";
    std::cout << "  podradio -e <file>        Export to OPML file\n";
    std::cout << "  podradio -t <time>        Sleep timer (see below)\n";
    std::cout << "  podradio --purge          Clear all cached data\n";
    std::cout << "  podradio -h, -?           Show this help\n\n";
    std::cout << "Sleep Timer Formats (-t):\n";
    std::cout << "  5h, 30m, 90s    With suffix (hours/minutes/seconds)\n";
    std::cout << "  1:30:00         HH:MM:SS format\n";
    std::cout << "  5               Auto: <100 = hours (5 hours)\n";
    std::cout << "  100             Auto: >=100 = minutes (100 min)\n\n";
    std::cout << "Data Paths:\n";
    std::cout << "  Database:   ~/.local/share/podradio/podradio.db (SQLite3)\n";
    std::cout << "  Config:     ~/.config/podradio/config.ini\n";
    std::cout << "  Downloads:  ~/Downloads/PodRadio/\n";
    std::cout << "  Log:        ~/.local/share/podradio/podradio.log\n\n";
    std::cout << "Database Tables:\n";
    std::cout << "  play_history  - Play history (max " << IniConfig::instance().get_history_max_days() << " days / "
              << IniConfig::instance().get_history_max_records() << " records)\n";
    std::cout << "  subscriptions - Podcast subscriptions\n";
    std::cout << "  radio_cache   - Radio directory cache\n";
    std::cout << "  youtube_cache - YouTube channel cache\n";
    std::cout << "  url_cache     - URL cache/download status\n";
    std::cout << "  favourites    - Favourite items\n";
    std::cout << "  search_cache  - iTunes search cache\n";
    std::cout << "  podcast_cache - Podcast info cache\n";
    std::cout << "  episode_cache - Episode list cache\n\n";
    std::cout << "Compile:\n";
    std::cout << "  g++ -std=c++17 podradio.cpp -o podradio \\\n";
    std::cout << "      -I/usr/include/libxml2 \\\n";
    std::cout << "      -lmpv -lncurses -lcurl -lxml2 -lfmt -lpthread -lsqlite3\n";
}

// =========================================================
// main - Application entry point
// =========================================================
int main(int argc, char* argv[]) {
    int opt;
    std::string import_url, export_file, import_opml_file, sleep_time;
    bool purge = false;

    // Support long options --purge and -h/-?
    static struct option long_options[] = {
        {"purge", no_argument, 0, 'P'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "a:i:e:t:h?", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a':
                import_url = optarg;
                break;
            case 'i':
                import_opml_file = optarg;
                break;
            case 'e':
                export_file = optarg;
                break;
            case 't':
                sleep_time = optarg;
                break;
            case 'h':
            case '?':
                print_usage();
                return 0;
            case 'P':
                purge = true;
                break;
        }
    }

    // Handle --purge to clear cache
    if (purge) {
        const char* home = std::getenv("HOME");
        if (home) {
            std::string cache_dir = std::string(home) + "/.local/share/podradio";
            std::cout << "Purging cache: " << cache_dir << std::endl;
            try {
                std::uintmax_t removed = fs::remove_all(cache_dir);
                std::cout << "Removed " << removed << " files/directories." << std::endl;
                std::cout << "Cache cleared successfully." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
        return 0;
    }

    curl_global_init(CURL_GLOBAL_ALL);
    xmlInitParser();

    // Set XML error handler immediately after xmlInitParser()
    xmlSetGenericErrorFunc(NULL, podradio::xml_error_handler);
    xmlSetStructuredErrorFunc(NULL, podradio::xml_structured_error_handler);

    // Set sleep timer
    if (!sleep_time.empty()) {
        int seconds = SleepTimer::parse_time_string(sleep_time);
        if (seconds > 0) {
            SleepTimer::instance().set_duration(seconds);
            std::cout << "Sleep timer set for " << seconds << " seconds" << std::endl;
        } else {
            std::cerr << "Invalid sleep time format: " << sleep_time << std::endl;
        }
    }

    // Save terminal state, register signal handlers, atexit cleanup
    podradio::save_terminal_state();
    podradio::setup_signal_handlers();
    atexit(podradio::tui_cleanup);

    // CLI mode: import/export operations
    if (!import_url.empty() || !export_file.empty() || !import_opml_file.empty()) {
        App app;
        // CLI mode needs to load persistent data, otherwise -e exports empty
        app.load_data();

        if (!import_url.empty()) {
            std::cout << "Importing: " << import_url << std::endl;
            app.import_feed(import_url);
            std::cout << "Waiting for feed to load..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        if (!import_opml_file.empty()) {
            app.import_opml(import_opml_file);
            std::cout << "Waiting for feeds to load..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        if (!export_file.empty()) {
            std::cout << "Exporting to: " << export_file << std::endl;
            app.export_podcasts(export_file);
        }
    } else {
        // TUI mode
        App app;
        app.run();
    }

    curl_global_cleanup();
    xmlCleanupParser();

    return 0;
}
