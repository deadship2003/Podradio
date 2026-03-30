#include "podradio/statusbar_color.h"

namespace podradio {

// =========================================================
// StatusBarColorRenderer - Implementation
// =========================================================

int StatusBarColorRenderer::get_color(const StatusBarColorConfig& config, int offset) {
    static int hue = 0;
    static auto last_update = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();

    // Advance hue when the configured update interval has elapsed
    if (elapsed >= config.update_interval_ms) {
        hue = (hue + config.rainbow_speed) % 360;
        last_update = now;
    }

    float brightness = calculate_brightness(config);

    switch (config.mode) {
        case StatusBarColorMode::RAINBOW:
            return get_rainbow_color(hue + offset, brightness);
        case StatusBarColorMode::RANDOM:
            return get_random_color(brightness);
        case StatusBarColorMode::TIME_BRIGHTNESS:
            return get_time_adjusted_color(brightness);
        case StatusBarColorMode::FIXED:
            return get_fixed_color(config.fixed_color, brightness);
        default:
            // CUSTOM and unknown modes fall back to rainbow
            return get_rainbow_color(hue + offset, brightness);
    }
}

// =========================================================

float StatusBarColorRenderer::calculate_brightness(const StatusBarColorConfig& config) {
    float brightness = config.brightness_max;

    // Adjust brightness based on time of day when time_adjust is enabled
    if (config.time_adjust) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&t);
        int hour = tm->tm_hour;

        if (hour >= 22 || hour < 6) {
            // Night time (22:00 - 05:59): use minimum brightness
            brightness = config.brightness_min;
        } else if (hour >= 18 || hour < 8) {
            // Dawn/dusk (06:00-07:59, 18:00-19:59): use midpoint brightness
            brightness = (config.brightness_min + config.brightness_max) / 2;
        }
        // Daytime (08:00 - 17:59): keep maximum brightness (already set above)
    }

    return brightness;
}

// =========================================================

int StatusBarColorRenderer::get_rainbow_color(int hue, float brightness) {
    // Convert hue (degrees) to HSV sector index and fractional part
    float h = hue / 60.0f;
    int i = static_cast<int>(h) % 6;
    float f = h - static_cast<int>(h);

    float v = brightness;
    float s = 1.0f;  // Full saturation for vivid rainbow colors

    // HSV to RGB intermediate values
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    // Map HSV sector to RGB components
    float r, g, b;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    // Map RGB (0.0-1.0) to ncurses 256-color cube (6 levels per channel)
    // Color cube starts at index 16: 16 + r*36 + g*6 + b
    return 16
         + static_cast<int>(r * 5) * 36
         + static_cast<int>(g * 5) * 6
         + static_cast<int>(b * 5);
}

// =========================================================

int StatusBarColorRenderer::get_random_color(float /*brightness*/) {
    // Use static RNG to avoid re-seeding on every call
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(16, 231);

    return dis(gen);
}

// =========================================================

int StatusBarColorRenderer::get_time_adjusted_color(float brightness) {
    // Derive hue from wall-clock seconds; hue completes a full cycle every 3600 seconds (1 hour)
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    int hue = (seconds / 10) % 360;

    return get_rainbow_color(hue, brightness);
}

// =========================================================

int StatusBarColorRenderer::get_fixed_color(const std::string& color_name, float /*brightness*/) {
    // Map of recognized color names to ncurses color constants
    static const std::map<std::string, int> color_map = {
        {"black",   COLOR_BLACK},
        {"red",     COLOR_RED},
        {"green",   COLOR_GREEN},
        {"yellow",  COLOR_YELLOW},
        {"blue",    COLOR_BLUE},
        {"magenta", COLOR_MAGENTA},
        {"cyan",    COLOR_CYAN},
        {"white",   COLOR_WHITE},
    };

    auto it = color_map.find(color_name);
    if (it != color_map.end()) {
        return it->second;
    }

    // Default to cyan for unrecognized color names
    return COLOR_CYAN;
}

} // namespace podradio
