#include "podradio/common.h"

#ifndef _WIN32
    #include <unistd.h>
    #include <termios.h>
    #include <sys/ioctl.h>
#else
    #include <windows.h>
#endif

namespace podradio
{

    // =========================================================
    // Global variable definitions
    // =========================================================

    // Global icon style (default Emoji) - switchable at runtime with U key
    IconStyle g_icon_style = IconStyle::EMOJI;

    // XML error handling state
    std::string g_last_xml_error;
    int g_xml_error_count = 0;

    // Terminal state
#ifndef _WIN32
    struct termios g_original_termios;
#endif
    bool g_termios_saved = false;
    bool g_ncurses_initialized = false;
    int g_original_lines = 0;
    int g_original_cols = 0;

    // Global exit request flag (for SIGINT)
    std::atomic<bool> g_exit_requested{false};

    // =========================================================
    // Terminal state save/restore functions
    // =========================================================

    // Save original terminal attributes
    void save_terminal_state() {
#ifndef _WIN32
        if (tcgetattr(STDIN_FILENO, &g_original_termios) == 0) {
            g_termios_saved = true;
        }
        struct winsize ws;
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
            g_original_lines = ws.ws_row;
            g_original_cols = ws.ws_col;
        }
#else
        // Windows: terminal state saved by PDCurses / Win32 console API
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
            g_original_lines = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            g_original_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        }
        g_termios_saved = true;
#endif
    }

    // Restore original terminal attributes
    void restore_terminal_state() {
        if (g_termios_saved) {
#ifndef _WIN32
            tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
#endif
            g_termios_saved = false;
        }
    }

    // Terminal cleanup function (for atexit and signal handling)
    // FIX: Use write() instead of printf() for signal safety
    void tui_cleanup() {
        if (g_ncurses_initialized) {
            endwin();
            g_ncurses_initialized = false;
        }
        // Restore cursor
        const char cursor_on[] = "\033[?25h";
#ifndef _WIN32
        ::write(STDOUT_FILENO, cursor_on, sizeof(cursor_on) - 1);
#else
        DWORD written;
        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), cursor_on, sizeof(cursor_on) - 1, &written, NULL);
#endif
        restore_terminal_state();
        fflush(stdout);
        fflush(stderr);
    }

    // =========================================================
    // Signal handler functions
    // =========================================================

    // Signal handler (SIGINT sets flag, others clean up and re-raise)
    void signal_handler(int sig) {
        if (sig == SIGINT) {
            // CTRL+C does not exit directly, sets flag for main loop to handle
            g_exit_requested = true;
            return;
        }
        // Other signals: normal cleanup and exit
        tui_cleanup();
        signal(sig, SIG_DFL);
        raise(sig);
    }

    // Register signal handlers for SIGINT, SIGTERM, SIGQUIT, SIGSEGV, SIGABRT
    void setup_signal_handlers() {
        signal(SIGINT, signal_handler);
#ifndef _WIN32
        signal(SIGTERM, signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGSEGV, signal_handler);
        signal(SIGABRT, signal_handler);
#else
        // Windows: handle Ctrl+C and Ctrl+Break
        signal(SIGTERM, signal_handler);
        signal(SIGABRT, signal_handler);
#endif
    }

} // namespace podradio
