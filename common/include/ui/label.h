/**
 * @file label.h
 * @brief 文本标签控件，用于显示不可交互的文本。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

/**
 * @brief 静态文本标签，显示单行或多行文本。
 */
class label : public widget {
public:
    std::string text;
    float font_size = 14.0f;
    text_alignment h_align = text_alignment::left;
    vertical_alignment v_align = vertical_alignment::center;

    /// @brief 是否允许鼠标选择文本。
    bool selectable = false;

    /// @brief 当前是否有非空选择。
    bool has_selection() const { return selecting_ && sel_anchor_ != sel_pos_; }

    /// @brief 返回选中文本。
    std::string selected_text() const;

    /// @brief 清除当前选择。
    void clear_selection();

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

    std::shared_ptr<renderer> cached_renderer_;
};

}
