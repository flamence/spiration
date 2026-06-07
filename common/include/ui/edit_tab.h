/**
 * @file edit_tab.h
 * @brief 代码编辑器标签页，支持语法高亮、行号、大文件。
 * @author clk
 */

#pragma once

#include <ui/tab_bar.h>
#include <utils/text_document.h>
#include <utils/syntax_highlighter.h>

namespace spiration {

/**
 * @brief 编辑器标签页。
 *
 * 核心特性：
 * - 语法高亮（行级缓存）
 * - 行号显示
 * - 虚拟渲染（仅处理可见行）
 * - 高性能（大文件 10M+ 无压力）
 */
class edit_tab : public tab {
public:
    edit_tab();
    ~edit_tab() override = default;

    
    bool open(const std::string& path);

    
    void new_file(const std::string& title = "untitled");

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;

    text_document& document() { return doc_; }
    const std::string& filepath() const { return filepath_; }

private:
    text_document doc_;
    syntax_highlighter highlighter_;
    std::string filepath_;

    
    float scroll_y_ = 0.0f;
    float scroll_max_ = 0.0f;

    
    float gutter_width() const {
        size_t lines = doc_.line_count();
        int digits = 1;
        while (lines >= 10) { digits++; lines /= 10; }
        return std::max(30.0f, static_cast<float>(digits) * 10.0f + 12.0f);
    }

    float line_height_ = 18.0f;
    float char_width_ = 8.5f;  

    
    size_t cursor_line_ = 0;
    size_t cursor_col_ = 0;

    
    size_t anchor_line_ = 0, anchor_col_ = 0;
    bool has_selection_ = false;

    void draw_line_numbers(std::shared_ptr<renderer> r);
    void draw_text_lines(std::shared_ptr<renderer> r);
    void paint_selection(std::shared_ptr<renderer> renderer);
    void insert_char(char c);
    void copy_selection();
    void paste();
    std::string get_selection_text() const;
};

} 
