#pragma once

#include "common.h"

namespace podradio {

// =========================================================
// StatusBarColorRenderer
// =========================================================
// Manages color rendering for the status bar with support for
// multiple color modes: rainbow, random, time-based brightness,
// fixed (named colors), and custom.
//
// Color modes:
//   RAINBOW           - HSV hue cycling across the status bar
//   RANDOM            - Random ncurses color each update tick
//   TIME_BRIGHTNESS   - Hue derived from current time, brightness by hour
//   FIXED             - Named ncurses color (black/red/green/yellow/blue/magenta/cyan/white)
//   CUSTOM            - Reserved for future use (falls back to rainbow)
//
// All methods are static; the class is stateless externally.
// Internal state (hue, last_update, RNG) is captured via static locals.

class StatusBarColorRenderer {
public:
    // Retrieve the ncurses color pair index for a given character offset.
    // @param config  The status bar color configuration (mode, speed, brightness, etc.)
    // @param offset  Character position offset within the status bar (used for rainbow spread)
    // @return        An ncurses color index (16-231 range for 256-color mode)
    static int get_color(const StatusBarColorConfig& config, int offset);

private:
    // Compute the current brightness factor based on time-of-day settings.
    // When time_adjust is enabled:
    //   22:00 - 05:59  -> brightness_min  (night)
    //   18:00 - 19:59  -> midpoint         (dusk)
    //   06:00 - 07:59  -> midpoint         (dawn)
    //   08:00 - 17:59  -> brightness_max   (day)
    // @param config  Color configuration containing brightness range and time_adjust flag
    // @return        Brightness value in [brightness_min, brightness_max]
    static float calculate_brightness(const StatusBarColorConfig& config);

    // Convert an HSV hue (0-360) and brightness (0.0-1.0) to an ncurses 256-color index.
    // Uses HSV->RGB with full saturation, then maps each channel to a 6-level scale.
    // @param hue         Hue value in degrees (0-360, will wrap around)
    // @param brightness  Value (brightness) component (0.0-1.0)
    // @return            Ncurses color index in range [16, 231]
    static int get_rainbow_color(int hue, float brightness);

    // Generate a random ncurses color index in the 256-color palette.
    // @param brightness  Unused (reserved for future brightness adjustment)
    // @return            Random color index in range [16, 231]
    static int get_random_color(float brightness);

    // Derive a hue from the current wall-clock time and convert to an ncurses color.
    // Hue advances every 10 seconds, wrapping at 360.
    // @param brightness  Brightness value to apply
    // @return            Ncurses color index in range [16, 231]
    static int get_time_adjusted_color(float brightness);

    // Look up a named ncurses color and return its color pair constant.
    // Recognized names: black, red, green, yellow, blue, magenta, cyan, white.
    // Falls back to COLOR_CYAN for unrecognized names.
    // @param color_name  Named color string (lowercase)
    // @param brightness  Unused (reserved for future brightness adjustment)
    // @return            Ncurses color constant (COLOR_BLACK, COLOR_RED, etc.)
    static int get_fixed_color(const std::string& color_name, float brightness);
};

} // namespace podradio
