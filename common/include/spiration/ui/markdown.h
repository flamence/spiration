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
    float draw_image_run(std::shared_ptr<renderer> r, const run& img, float x, float y,
                         float font_size, float line_height) const;
    std::string link_at(float x, float y) const;

    /// @brief 代码块/表格的自然宽度（含内边距）。
    float block_natural_width(std::shared_ptr<renderer> r, const block& b) const;
    /// @brief 某块可滚动区域的横向最大滚动距离。
    float block_scroll_max(size_t bindex, float avail_w) const;

    /**
     * @brief 排版后的行内片段：x/w 已在排版期算好，paint 期只按位置绘制。
     */
    struct placed_run {
        float x = 0.0f;                 ///< 相对行起点的 x 偏移
        float w = 0.0f;                 ///< 片段宽度
        std::string text;
        std::string href;
        size_t src = 0;
        run_style style = run_style::normal;
        float font_size = 14.0f;
        std::string family;
    };

    /// @brief 排版后的一行（或表格单元格的一行）。
    struct placed_line {
        std::vector<placed_run> runs;
        float width = 0.0f;             ///< 行内片段总宽（与原 line_width 口径一致）
        float space_w = 0.0f;           ///< 片段间空格宽度（排版期测得，绘制期复用）
    };

    /**
     * @brief 单个块的排版缓存。
     * 失效键：所属解析结果（blocks_ 重建时整体清空）、排版宽度 avail_w、
     *         块内实际字号 font_size、UI 字体族 family、等宽字体族 mono_family。
     */
    struct block_layout {
        float avail_w = -1.0f;
        float font_size = -1.0f;
        std::string family;
        std::string mono_family;
        std::vector<placed_line> lines;                     ///< 标题/段落/引用
        std::vector<std::vector<placed_line>> items;        ///< 列表：每项的行
        std::vector<float> item_marker_w;                   ///< 列表：每项标记宽度
        std::vector<placed_line> table_cells;               ///< 表格：按行优先排列的单元格
        std::vector<float> col_w;                           ///< 表格列宽
        std::vector<float> code_line_w;                     ///< 代码块每行宽度
        float natural_w = 0.0f;                             ///< 代码块/表格自然宽度
        bool valid = false;
    };

    /// @brief 取某块的排版缓存，键变化时重建（每个块每帧至多一次，命中时零测量）。
    const block_layout& ensure_block_layout(std::shared_ptr<renderer> r, const block& b,
                                            size_t bindex, float avail_w,
                                            const std::string& family,
                                            const std::string& mono_family) const;
    /// @brief 构建单个块的排版缓存。
    void build_block_layout(std::shared_ptr<renderer> r, const block& b, float avail_w,
                            const std::string& family, const std::string& mono_family,
                            block_layout& out) const;
    /// @brief 折行并生成带位置的排版行（测量只在此进行）。
    std::vector<placed_line> layout_inline(std::shared_ptr<renderer> r,
                                           const std::vector<run>& runs, float wrap_w,
                                           float font_size, const std::string& base_family) const;
    /// @brief 对不换行的一组 run 逐段测宽并生成一行排版结果（表格单元格用）。
    placed_line layout_single_line(std::shared_ptr<renderer> r, const std::vector<run>& runs,
                                   float font_size, const std::string& base_family) const;
    /// @brief 用已测好的每段宽度生成带位置的排版行。
    placed_line make_placed_line(const std::vector<run>& runs,
                                 const std::vector<float>& widths,
                                 float font_size, const std::string& base_family,
                                 float space_w) const;
    /// @brief 绘制一行已排版好的行内片段（不再测量文本宽度）。
    void draw_placed_line(std::shared_ptr<renderer> r, const placed_line& line, float x, float y,
                          float line_height, const color& base_color, const color& code_bg,
                          const color& link_color) const;

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
    /// @brief 各块排版缓存，下标与 blocks_ 对应；blocks_ 重建时清空。
    mutable std::vector<block_layout> block_layouts_;

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
