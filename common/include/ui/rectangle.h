/**
 * @file rectangle.h
 * @brief 矩形区域数据结构。
 * @author clk
 */

#pragma once

#include <ui/point.h>
#include <ui/size.h>

namespace spiration {

/**
 * @brief 矩形区域，由左上角坐标 (x, y) 和宽高定义。
 */
struct rectangle {
    float x, y;
    float width, height;

    rectangle(float x, float y, float width, float height) : x(x), y(y), width(width), height(height) {}
};

}