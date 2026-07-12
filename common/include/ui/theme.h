/**
 * @file theme.h
 * @brief 基于 profile 的统一主题管理。
 * @author clk
 */

#pragma once

#include <ui/color.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace spiration {

/**
 * @brief 主题管理。
 */
class theme {
public:
    static constexpr const char* WINDOW_BG          = "window_bg";
    static constexpr const char* APPBAR_BG          = "appbar_bg";
    static constexpr const char* MENU_BAR_BG        = "menu_bar_bg";
    static constexpr const char* MENU_TEXT          = "menu_text";
    static constexpr const char* BUTTON_TEXT        = "button_text";
    static constexpr const char* BUTTON_HOVER       = "button_hover";
    static constexpr const char* BUTTON_PRESS       = "button_press";
    static constexpr const char* POPUP_BG           = "popup_bg";
    static constexpr const char* POPUP_BORDER       = "popup_border";
    static constexpr const char* POPUP_HOVER        = "popup_hover";
    static constexpr const char* POPUP_TEXT         = "popup_text";
    static constexpr const char* CLOSE_HOVER        = "close_hover";
    static constexpr const char* CONTROL_ICON       = "control_icon";
    static constexpr const char* CONTROL_ICON_HOVER = "control_icon_hover";
    static constexpr const char* CONTROL_HOVER_BG   = "control_hover_bg";
    static constexpr const char* SEPARATOR          = "separator";
    static constexpr const char* EDITOR_BG          = "editor_bg";
    static constexpr const char* EDITOR_GUTTER_BG   = "editor_gutter_bg";
    static constexpr const char* EDITOR_LINE_NUM    = "editor_line_num";
    static constexpr const char* EDITOR_CURSOR      = "editor_cursor";

    static constexpr const char* TAB_BAR_BG          = "tab_bar_bg";
    static constexpr const char* TAB_ACTIVE_BG       = "tab_active_bg";
    static constexpr const char* TAB_ACTIVE_TEXT     = "tab_active_text";
    static constexpr const char* TAB_HOVER_BG        = "tab_hover_bg";
    static constexpr const char* TAB_HOVER_TEXT      = "tab_hover_text";
    static constexpr const char* TAB_INACTIVE_BG     = "tab_inactive_bg";
    static constexpr const char* TAB_INACTIVE_TEXT   = "tab_inactive_text";
    static constexpr const char* TAB_INDICATOR       = "tab_indicator";
    static constexpr const char* TAB_CLOSE_FG        = "tab_close_fg";
    static constexpr const char* TAB_CLOSE_HOVER_FG  = "tab_close_hover_fg";

    static void set_active(const std::string& name);
    static std::string active();
    static std::vector<std::string> profiles();
    static void register_profile(const std::string& name);

    static color get(const std::string& key);

    static void set(const std::string& key, const color& value);
    static void set(const std::string& profile, const std::string& key, const color& value);

private:
    struct profile {
        std::string name;
        std::unordered_map<std::string, color> params;
    };

    static std::vector<profile> s_profiles;
    static size_t s_active;
    static void ensure_profiles();
    static void init_defaults(profile* dark);
};

} 
