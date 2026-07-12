/**
 * @file margin.h
 * @brief 边距数据结构。
 * @author clk
 */

#pragma once

namespace spiration {

/**
 * @brief 四周边距，支持多种构造方式。
 */
class margin {
public:
    margin() : top(0), right(0), bottom(0), left(0) {}
    margin(int all) : top(all), right(all), bottom(all), left(all) {}
    margin(int vertical, int horizontal) 
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    margin(int t, int r, int b, int l) 
        : top(t), right(r), bottom(b), left(l) {}
    
    int top, right, bottom, left;
};

}