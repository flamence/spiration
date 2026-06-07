/**
 * @file theme.h
 * @brief 统一主题管理，定义配色方案和 UI 常量。
 * @author clk
 */

#pragma once

#include <ui/color.h>

namespace spiration {

/**
 * @brief 主题配色与样式常量集合。
 *
 * 所有 widget 通过 theme::xxx() 获取颜色，统一管理。
 */
class theme {
public:
    
    static color window_bg()          { return { 1.0f, 1.0f, 1.0f }; }

    
    static color appbar_bg()          { return { 0.55f, 0.55f, 0.55f }; }

    
    static color menu_bar_bg()        { return { 0.0f, 0.0f, 0.0f, 0.0f }; }

    
    static color menu_text()          { return { 1.0f, 1.0f, 1.0f }; }

    
    static color button_text()        { return { 1.0f, 1.0f, 1.0f }; }

    
    static color button_hover()       { return { 0.0f, 0.0f, 0.0f, 0.45f }; }

    
    static color button_press()       { return { 0.0f, 0.0f, 0.0f, 0.65f }; }

    
    static color popup_bg()           { return { 0.97f, 0.97f, 0.97f }; }

    
    static color popup_border()       { return { 0.75f, 0.75f, 0.75f }; }

    
    static color popup_hover()        { return { 0.2f, 0.4f, 0.8f, 0.25f }; }

    
    static color popup_text()         { return { 0.1f, 0.1f, 0.1f }; }

    
    static color close_hover()        { return { 0.85f, 0.2f, 0.2f }; }

    
    static color control_icon()       { return { 0.0f, 0.0f, 0.0f }; }

    
    static color control_icon_hover() { return { 1.0f, 1.0f, 1.0f }; }

    
    static color control_hover_bg()   { return { 0.0f, 0.0f, 0.0f, 0.12f }; }

    
    static color separator()          { return { 0.8f, 0.8f, 0.8f }; }

    
    static color editor_bg()          { return { 0.92f, 0.92f, 0.90f }; }

    
    static color editor_gutter_bg()   { return { 0.82f, 0.82f, 0.80f }; }

    
    static color editor_line_num()    { return { 0.45f, 0.45f, 0.45f }; }

    
    static color editor_cursor()      { return { 0.2f, 0.2f, 0.2f }; }
};

} 
