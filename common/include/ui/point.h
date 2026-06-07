/**
 * @file point.h
 * @brief 二维坐标点数据结构。
 * @author clk
 */

#pragma once

namespace spiration {

/**
 * @brief 二维空间中的坐标点 (x, y)。
 */
struct point {
    float x = 0.0f;
    float y = 0.0f;
    
    point() = default;
    point(float x, float y) : x(x), y(y) {}
};

}