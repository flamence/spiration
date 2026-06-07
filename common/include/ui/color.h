/**
 * @file color.h
 * @brief RGBA 颜色数据结构。
 * @author clk
 */

#pragma once

namespace spiration {

/**
 * @brief RGBA 颜色表示，各通道范围为 [0.0, 1.0]。
 */    
struct color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    
    color() = default;
    color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    color(const color&) = default;

    bool operator==(const color& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const color& o) const { return !(*this == o); }
    
    static color transparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static color black() { return {0.0f, 0.0f, 0.0f}; }
    static color white() { return {1.0f, 1.0f, 1.0f}; }
    static color red() { return {1.0f, 0.0f, 0.0f}; }
    static color green() { return {0.0f, 1.0f, 0.0f}; }
    static color blue() { return {0.0f, 0.0f, 1.0f}; }
};

}