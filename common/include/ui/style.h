/**
 * @file style.h
 * @brief 控件样式定义。
 * @author clk
 */

#pragma once

#include "color.h"
#include "cursor.h"
#include "margin.h"
#include "font.h"
#include <string>

namespace spiration {

/**
 * @brief 控件的视觉样式集合。
 */
class style {
public:
    spiration::color background_color;
    spiration::color textColor;
    spiration::font font;
    spiration::margin margin;
    spiration::margin padding;
    spiration::color border_color;
    int borderWidth = 0;
    int width = 0;
    int height = 0;
    std::string display;
    std::string position;

    ///< 鼠标悬停在此控件上时显示的光标形状。
    spiration::cursor_type cursor = spiration::cursor_type::default_cursor;

    ///< 是否允许水平滚动。
    bool overflow_x = false;
    ///< 是否允许垂直滚动。
    bool overflow_y = false;
};

}