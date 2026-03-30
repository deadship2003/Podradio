#include "podradio/common.h"

#include <unistd.h>  // write()

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
    struct termios g_original_termios;
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
        if (tcgetattr(STDIN_FILENO, &g_original_termios) == 0) {
            g_termios_saved = true;
        }
        // Save original terminal size
        struct winsize ws;
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
            g_original_lines = ws.ws_row;
            g_original_cols = ws.ws_col;
        }
    }

    // Restore original terminal attributes
    void restore_terminal_state() {
        if (g_termios_saved) {
            tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
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
        // Restore cursor using write() for signal safety
        const char cursor_on[] = "\033[?25h";
        ::write(STDOUT_FILENO, cursor_on, sizeof(cursor_on) - 1);
        // Reset terminal attributes
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
    // FIX: Use sigaction() for SIGTERM/SIGQUIT for more reliable handling
    void setup_signal_handlers() {
        // SIGINT: use signal() since it only sets a flag
        signal(SIGINT, signal_handler);

        // SIGTERM: use sigaction() for reliable handler
        struct sigaction sa_term;
        sigemptyset(&sa_term.sa_mask);
        sa_term.sa_flags = 0;
        sa_term.sa_handler = signal_handler;
        sigaction(SIGTERM, &sa_term, nullptr);

        // SIGQUIT: use sigaction() for reliable handler
        struct sigaction sa_quit;
        sigemptyset(&sa_quit.sa_mask);
        sa_quit.sa_flags = 0;
        sa_quit.sa_handler = signal_handler;
        sigaction(SIGQUIT, &sa_quit, nullptr);

        // SIGSEGV: use sigaction()
        struct sigaction sa_segv;
        sigemptyset(&sa_segv.sa_mask);
        sa_segv.sa_flags = 0;
        sa_segv.sa_handler = signal_handler;
        sigaction(SIGSEGV, &sa_segv, nullptr);

        // SIGABRT: use sigaction()
        struct sigaction sa_abrt;
        sigemptyset(&sa_abrt.sa_mask);
        sa_abrt.sa_flags = 0;
        sa_abrt.sa_handler = signal_handler;
        sigaction(SIGABRT, &sa_abrt, nullptr);
    }

} // namespace podradio
