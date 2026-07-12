/**
 * @file size.h
 * @brief 尺寸数据结构。
 * @author clk
 */

#pragma once

namespace spiration {

/**
 * @brief 二维尺寸表示。
 */
struct size {
    float width = 0.0f;
    float height = 0.0f;
    
    size() = default;
    size(float width, float height) : width(width), height(height) {}
};

}