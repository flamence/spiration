/**
 * @file theme_manager.h
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
 * @brief 主题管理器。
 */
class theme_manager {
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
    static constexpr const char* CONTENT_BG         = "content_bg";
    static constexpr const char* SEPARATOR          = "separator";
    static constexpr const char* TEXT_MUTED         = "text_muted";

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

    static constexpr const char* LABEL_TEXT            = "label_text";
    static constexpr const char* CHECKBOX_BORDER        = "checkbox_border";
    static constexpr const char* CHECKBOX_CHECK_BG      = "checkbox_check_bg";
    static constexpr const char* CHECKBOX_CHECK_FG      = "checkbox_check_fg";
    static constexpr const char* TOGGLE_BG              = "toggle_bg";
    static constexpr const char* TOGGLE_BG_ACTIVE       = "toggle_bg_active";
    static constexpr const char* TOGGLE_KNOB            = "toggle_knob";
    static constexpr const char* SLIDER_TRACK           = "slider_track";
    static constexpr const char* SLIDER_FILL            = "slider_fill";
    static constexpr const char* SLIDER_THUMB           = "slider_thumb";
    static constexpr const char* SLIDER_THUMB_HOVER     = "slider_thumb_hover";
    static constexpr const char* INPUT_BG               = "input_bg";
    static constexpr const char* INPUT_BORDER           = "input_border";
    static constexpr const char* INPUT_FOCUS_BORDER     = "input_focus_border";
    static constexpr const char* INPUT_TEXT             = "input_text";
    static constexpr const char* INPUT_PLACEHOLDER      = "input_placeholder";
    static constexpr const char* INPUT_CURSOR           = "input_cursor";
    static constexpr const char* PROGRESS_BG            = "progress_bg";
    static constexpr const char* PROGRESS_FILL          = "progress_fill";

    static constexpr const char* TOOLTIP_BG             = "tooltip_bg";
    static constexpr const char* TOOLTIP_TEXT           = "tooltip_text";
    static constexpr const char* COMBO_BG               = "combo_bg";
    static constexpr const char* COMBO_BORDER           = "combo_border";
    static constexpr const char* COMBO_ARROW            = "combo_arrow";
    static constexpr const char* SPLIT_HANDLE           = "split_handle";
    static constexpr const char* SPLIT_HANDLE_HOVER     = "split_handle_hover";
    static constexpr const char* DIALOG_BG              = "dialog_bg";
    static constexpr const char* DIALOG_OVERLAY         = "dialog_overlay";
    static constexpr const char* LIST_BG                = "list_bg";
    static constexpr const char* LIST_ITEM_HOVER        = "list_item_hover";
    static constexpr const char* LIST_ITEM_SELECTED     = "list_item_selected";
    static constexpr const char* SCROLL_BAR_BG          = "scroll_bar_bg";
    static constexpr const char* SCROLL_BAR_THUMB       = "scroll_bar_thumb";
    static constexpr const char* SCROLL_BAR_THUMB_HOVER = "scroll_bar_thumb_hover";

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

} // namespace spiration
