/**
 * @file font.h
 * @brief 字体属性定义（字体族、大小、粗体/斜体）。
 * @author clk
 */

#pragma once

#include <string>

namespace spiration {

/**
 * @brief 字体属性集合。
 */
class font {
public:
    font() : size(14), bold(false), italic(false) {}
    font(const std::string& family, int size = 14) 
        : family(family), size(size), bold(false), italic(false) {}
    
    std::string family;
    int size;
    bool bold;
    bool italic;
};

}