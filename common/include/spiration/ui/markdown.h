/**
 * @file markdown.h
 * @brief Markdown 文本控件。
 * @author clk
 */

#pragma once

#include <spiration/ui/label.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

/**
 * @brief Markdown 文本控件。
 */
class markdown : public label {
public:
    /// @brief 行内文本样式。
    enum class run_style { normal, bold, italic, bold_italic, code, link, image };

    /// @brief 带样式的一段文本。
    struct run {
        std::string text;
        run_style style = run_style::normal;
        std::string href;
        size_t src = 0;
    };

    /// @brief 块级元素。
    struct block {
        enum class kind { heading, paragraph, code, quote, list, hr, table };
        kind k = kind::paragraph;
        int level = 0;
        bool ordered = false;
        std::vector<run> runs;
        std::vector<std::vector<run>> items;
        std::vector<std::string> code_lines;
        std::vector<size_t> code_offsets;
        std::vector<std::vector<std::vector<run>>> table;
    };

    markdown() = default;
    explicit markdown(const std::string& text);

    void paint(std::shared_ptr<renderer> renderer) override;
    void layout() override;
    size layout_preferred_size() const override;
    void handle_event(const event_type& type, void* data) override;
    cursor_style effective_cursor(float lx, float ly) const override;

protected:
    size_t hit_test_text(float x, float y) const override;
    void draw_selection_highlight(std::shared_ptr<renderer> r) const override;
    /// @brief 点是否落在渲染出的文本可视区域内。
    bool hit_text_area(float x, float y) const override;

private:
    std::vector<block> parse(const std::string& src) const;
    static std::vector<run> parse_inline(const std::string& line, size_t line_off,
                                         const std::map<std::string, std::string>& refs);
    /// @brief 解析一行表格。
    static std::vector<std::vector<run>> parse_table_row(
        const std::string& line, size_t line_off,
        const std::map<std::string, std::string>& refs);

    void ensure_parsed() const;
    std::shared_ptr<renderer> measure_renderer() const;
    float effective_width() const;
    float measure_height(std::shared_ptr<renderer> r, float avail_w) const;
    /// @brief 计算表格按自然列宽展开后的总宽度。
    static float table_natural_width(
        std::shared_ptr<renderer> r,
        const std::vector<std::vector<std::vector<run>>>& table,
        float font_size, const std::string& family,
        std::vector<float>* col_w_out = nullptr);
    void draw_block(std::shared_ptr<renderer> r, const block& b, size_t bindex,
                    float x, float y, float avail_w, float& y_out) const;
    void draw_word_line(std::shared_ptr<renderer> r, const std::vector<run>& line,
                        float x, float y, float font_size, const std::string& base_family,
                        const color& base_color, const color& code_bg, float line_height,
                        const color& link_color) const;
    float draw_image_run(std::shared_ptr<renderer> r, const run& img, float x, float y,
                         float font_size, float line_height) const;
    std::string link_at(float x, float y) const;

    /// @brief 代码块/表格的自然宽度（含内边距）。
    float block_natural_width(std::shared_ptr<renderer> r, const block& b) const;
    /// @brief 某块可滚动区域的横向最大滚动距离。
    float block_scroll_max(size_t bindex, float avail_w) const;

    struct word_rect {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        size_t start = 0, end = 0;
        std::string href;
        std::string text;
        run_style style = run_style::normal;
        float font_size = 14.0f;
    };

    static size_t word_offset_at(const word_rect& wr, float rel_x, std::shared_ptr<renderer> r);
    static float word_prefix_width(const word_rect& wr, size_t byte_len, std::shared_ptr<renderer> r);
    mutable std::vector<word_rect> word_rects_;

    mutable std::string parsed_text_;
    mutable std::vector<block> blocks_;
    mutable std::shared_ptr<renderer> cached_renderer_;

    mutable std::string cached_h_text_;
    mutable float cached_h_width_ = -1.0f;
    mutable float cached_h_font_ = -1.0f;
    mutable float cached_h_height_ = 0.0f;

    std::string pressed_link_;

    /// @brief 各代码块/表格的横向滚动偏移，与 blocks_ 下标对应。
    mutable std::vector<float> block_scroll_x_;
    /// @brief 各代码块/表格的可见矩形（本控件坐标），仅可滚动块有效。
    mutable std::vector<rectangle> block_rects_;
    /// @brief 鼠标悬停的可滚动块下标，无则 -1。
    int hover_block_ = -1;
    /// @brief 正在拖拽滚动条的可滚动块下标，无则 -1。
    int drag_block_ = -1;
    float drag_start_x_ = 0.0f;
    float drag_start_off_ = 0.0f;
};

}
