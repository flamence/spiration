/**
 * @file syntax_highlighter.h
 * @brief 代码语法高亮器，支持 C/C++ 基本语法，行级缓存。
 * @author clk
 */

#pragma once

#include <ui/color.h>
#include <string>
#include <vector>
#include <unordered_set>

namespace spiration {

/**
 * @brief 单个语法标记。
 */
struct syntax_token {
    size_t start = 0;  
    size_t len = 0;    
    color col;         
};

/**
 * @brief 语法高亮器，行级缓存，仅重新高亮变更行。
 */
class syntax_highlighter {
public:
    syntax_highlighter();

    
    const std::vector<syntax_token>& highlight_line(size_t n, const std::string& line_text);

    
    void invalidate_line(size_t n);

    
    void invalidate_all();

    
    void set_language(const std::string& lang);

    
    color comment_color() const { return {0.25f, 0.55f, 0.25f}; }
    color keyword_color() const { return {0.1f, 0.25f, 0.7f}; }
    color string_color() const { return {0.75f, 0.2f, 0.15f}; }
    color number_color() const { return {0.1f, 0.55f, 0.55f}; }
    color preprocessor_color() const { return {0.45f, 0.25f, 0.75f}; }
    color default_color() const { return {0.15f, 0.15f, 0.15f}; }

private:
    std::string language_ = "cpp";
    std::unordered_set<std::string> keywords_;
    std::vector<std::vector<syntax_token>> line_cache_;
    std::vector<bool> line_valid_;

    void build_keywords();
    void tokenize_line(size_t n, const std::string& text);
};

} 
