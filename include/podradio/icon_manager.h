#ifndef PODRADIO_ICON_MANAGER_H
#define PODRADIO_ICON_MANAGER_H

#include "common.h"

#include <string>

namespace podradio {

// Note: SAFE_BORDER_MARGIN, ICON_FIELD_WIDTH, EMOJI_LOGICAL_WIDTH, ASCII_LOGICAL_WIDTH
// are defined in common.h within this namespace.


//图标管理器 - 支持ASCII/Emoji双模式
// 图标格式：<图标><空格>，与connector之间保持1空格分隔
class IconManager {
public:
    // V0.05B9n3e5g3h: 设置图标风格
    static inline void set_style(IconStyle style) { g_icon_style = style; }
    static inline IconStyle get_style() { return g_icon_style; }
    static inline void toggle_style() {
        g_icon_style = (g_icon_style == IconStyle::EMOJI) ? IconStyle::ASCII : IconStyle::EMOJI;
    }
    static inline std::string get_style_name() {
        return (g_icon_style == IconStyle::EMOJI) ? "Emoji" : "ASCII";
    }

    // 节点状态图标
    static inline std::string get_folder_expanded() {
        return (g_icon_style == IconStyle::EMOJI) ? "▼ " : "v ";
    }
    static inline std::string get_folder_collapsed() {
        return (g_icon_style == IconStyle::EMOJI) ? "▶ " : "> ";
    }
    static inline std::string get_marked() {
        return (g_icon_style == IconStyle::EMOJI) ? "🔖 " : "* ";
    }
    static inline std::string get_failed() {
        return (g_icon_style == IconStyle::EMOJI) ? "✗ " : "x ";
    }
    static inline std::string get_loading() {
        return (g_icon_style == IconStyle::EMOJI) ? "⏳ " : "~ ";
    }

    // 节点类型图标
    static inline std::string get_radio() {
        return (g_icon_style == IconStyle::EMOJI) ? "🎵 " : "[R]";
    }
    static inline std::string get_podcast() {
        return (g_icon_style == IconStyle::EMOJI) ? "🎙 " : "[P]";
    }
    static inline std::string get_video() {
        return (g_icon_style == IconStyle::EMOJI) ? "📺 " : "[V]";
    }
    static inline std::string get_music() {
        return (g_icon_style == IconStyle::EMOJI) ? "♪ " : "[M]";
    }

    // 播放状态图标
    static inline std::string get_playing() {
        return (g_icon_style == IconStyle::EMOJI) ? "▶" : ">";
    }
    static inline std::string get_paused() {
        return (g_icon_style == IconStyle::EMOJI) ? "⏸" : "||";
    }
    static inline std::string get_stopped() {
        return (g_icon_style == IconStyle::EMOJI) ? "⏹" : "[]";
    }
    static inline std::string get_buffering() {
        return (g_icon_style == IconStyle::EMOJI) ? "⏳" : "~";
    }

    // 缓存状态图标
    static inline std::string get_cached() {
        return (g_icon_style == IconStyle::EMOJI) ? "✓" : "+";
    }
    static inline std::string get_downloading() {
        return (g_icon_style == IconStyle::EMOJI) ? "↓" : "d";
    }

    // 图标显示宽度
    static inline int get_icon_display_width() {
        return (g_icon_style == IconStyle::EMOJI) ? EMOJI_LOGICAL_WIDTH : ASCII_LOGICAL_WIDTH;
    }

    // 图标区域总宽度（包含空格）
    static inline int get_icon_field_width() {
        return (g_icon_style == IconStyle::EMOJI) ? ICON_FIELD_WIDTH : 2;  // ASCII: 1字符+空格=2
    }
};

} // namespace podradio

#endif // PODRADIO_ICON_MANAGER_H
