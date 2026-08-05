/**
 * @file combo_box.h
 * @brief 下拉选择框，点击展开选项列表。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme_manager.h>
#include <utils/animation.h>
#include <functional>
#include <string>
#include <vector>

namespace spiration {

/**
 * @brief 下拉选择框控件。
 */
class combo_box : public widget {
public:
    combo_box() {
        widget_style.cursor = cursor_type::pointer;
        focusable = true;
    }
    std::vector<std::string> items;
    int selected_index = -1;
    std::function<void(int)> on_changed;

    bool hit_test(float x, float y) const override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;

    /// @brief 失去焦点。
    void on_blur() override;

    float item_height = 24.0f;
    float font_size = 14.0f;

    /// @brief 弹出列表向上展开。
    bool popup_up = false;

private:
    bool expanded_ = false;
    bool hovering_ = false;
    int hovered_idx_ = -1;
    /// @brief 弹出项悬停背景过渡。
    color_transition hover_bg_{color::transparent()};
    float popup_height() const;
    /// @brief 考虑边界后的实际弹出高度。
    float effective_popup_height() const;

    void toggle();
    void close();
};

}
