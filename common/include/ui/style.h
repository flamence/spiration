/**
 * @file style.h
 * @brief 控件样式定义。
 * @author clk
 */

#pragma once

#include "color.h"
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
    int borderWidth;
    int width;
    int height;
    std::string display;
    std::string position;
};

}