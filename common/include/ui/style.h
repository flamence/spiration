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
 *
 * 包含背景色、文字颜色、字体、边距、边框等样式属性，
 * 为控件绘制提供配置参数。
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