/**
 * @file label.h
 * @brief 文本标签控件。
 * @author clk
 */

#pragma once

#include <spiration/ui/widget.h>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

/** @brief 固定宽度下文本溢出处理方式。 */
enum class text_overflow { none, ellipsis, hide };

/**
 * @brief 静态文本标签，显示单行或多行文本。
 */
class label : public widget {
public:
    std::string text;

    /**
     * @brief 设置文本并标记内容变更。
     * @param t 新文本内容
     */
    void set_text(const std::string& t) {
        text = t;
        invalidate_layout();
        if (request_repaint_) request_repaint_();
    }

    float font_size = 14.0f;
    text_alignment h_align = text_alignment::left;
    vertical_alignment v_align = vertical_alignment::center;

    /// @brief 固定宽度下文本溢出处理。
    text_overflow overflow = text_overflow::none;

    /// @brief 是否允许鼠标选择文本。
    bool selectable = false;

    /// @brief 测量当前文本的像素宽度。
    float text_width() const;

    /// @brief 当前是否有非空选择。
    bool has_selection() const { return selecting_ && sel_anchor_ != sel_pos_; }

    /// @brief 返回选中文本。
    std::string selected_text() const;

    /// @brief 清除当前选择。
    void clear_selection();

    void clear_text_selection() override { clear_selection(); }

    /// @brief 将控件内坐标映射为文本中的字节偏移（跨控件选取用）。
    size_t text_offset_at(float x, float y) const { return hit_test_text(x, y); }

    /// @brief 设置本控件的选择区间（跨控件选取用）。
    void set_selection_range(size_t a, size_t b) {
        selecting_ = true;
        sel_anchor_ = a;
        sel_pos_ = b;
        if (request_repaint_) request_repaint_();
    }

    /**
     * @brief 点 (x, y) 是否落在文本可视区域内。
     */
    virtual bool hit_text_area(float x, float y) const;

    cursor_style effective_cursor(float lx, float ly) const override {
        if (selectable)
            return hit_text_area(lx, ly) ? cursor_style::text : cursor_style::default_cursor;
        return widget_style.cursor;
    }

    void paint(std::shared_ptr<renderer> renderer) override;

    void layout() override;

    size layout_preferred_size() const override;

    void handle_event(const event_type& type, void* data) override;

protected:
    size_t sel_anchor_ = 0;
    size_t sel_pos_ = 0;
    bool selecting_ = false;
    bool mouse_down_ = false;

    /// @brief 将控件内坐标映射为 text 中的字节偏移。
    virtual size_t hit_test_text(float x, float y) const;

    /// @brief 绘制选择高亮。
    virtual void draw_selection_highlight(std::shared_ptr<renderer> r) const;

    /// @brief 在局部坐标 (mx,my) 处弹出右键上下文菜单。
    virtual void open_context_menu(float mx, float my);

private:
    /// @brief 折行后的视觉行。
    struct line_info {
        float y = 0.0f;
        float height = 0.0f;
        size_t start = 0;
        size_t end = 0;
        float width = 0.0f;
        std::vector<float> widths;
        std::vector<size_t> char_offsets;
    };

    std::shared_ptr<renderer> current_renderer() const;
    float align_x(float line_width, float wrap_width) const;
    float layout_lines(std::shared_ptr<renderer> r, std::vector<line_info>& out) const;

    /// @brief 非 selectable 分支实际要绘制的文本（含省略处理），带缓存。
    const std::string& display_text(const std::shared_ptr<renderer>& r) const;

    /// @brief 沿父链通知 root：本控件开始新的文本选择。
    void notify_root_selection_started();

    /// @brief 沿父链通知 root：本控件开始一次跨控件选择。
    void notify_root_cross_selection_begin(size_t offset);
    /// @brief 沿父链通知 root：拖拽过程中更新跨控件选择。
    void notify_root_cross_selection_move(float mx, float my);
    /// @brief 沿父链通知 root：拖拽结束。
    void notify_root_cross_selection_end();

    std::shared_ptr<renderer> cached_renderer_;

    mutable std::vector<line_info> cached_lines_;
    mutable float cached_lines_total_h_ = 0.0f;
    mutable std::string cached_lines_text_;
    mutable float cached_lines_width_ = -1.0f;
    mutable float cached_lines_font_ = -1.0f;
    mutable float cached_lines_height_ = -1.0f;
    mutable bool cached_lines_valid_ = false;

    /**
     * @brief 省略/隐藏后的绘制文本缓存。
     * 失效键：text、控件宽度 width、font_size、主题字体族、overflow。
     * 上述任一项变化才重新测量并省略；否则 paint 直接复用（省略逻辑内部会反复 measure）。
     */
    mutable std::string cached_display_text_;
    mutable std::string cached_display_key_text_;
    mutable std::string cached_display_key_family_;
    mutable float cached_display_key_width_ = -1.0f;
    mutable float cached_display_key_font_ = -1.0f;
    mutable text_overflow cached_display_key_overflow_ = text_overflow::none;
    mutable bool cached_display_valid_ = false;

    /**
     * @brief layout_preferred_size() 的测量结果缓存。
     * 失效键：text、font_size、主题字体族、目标宽度、是否有可用 renderer。
     * 该函数会被父布局在同一次 layout 中多次调用，缓存后只有首次真正测量。
     */
    mutable size cached_pref_size_;
    mutable std::string cached_pref_key_text_;
    mutable std::string cached_pref_key_family_;
    mutable float cached_pref_key_width_ = -1.0f;
    mutable float cached_pref_key_font_ = -1.0f;
    mutable bool cached_pref_key_has_renderer_ = false;
    mutable bool cached_pref_valid_ = false;

    /// @brief text_width() 的结果缓存（失效键：text、font_size、主题字体族）。
    mutable float cached_text_width_value_ = 0.0f;
    mutable std::string cached_text_width_key_text_;
    mutable std::string cached_text_width_key_family_;
    mutable float cached_text_width_key_font_ = -1.0f;
    mutable bool cached_text_width_valid_ = false;
};

}
