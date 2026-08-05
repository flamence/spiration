/**
 * @file slider.h
 * @brief 滑块控件，用于在范围内选择数值。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme_manager.h>
#include <functional>
#include <algorithm>

namespace spiration {

/**
 * @brief 水平滑块，支持拖拽选择 [min, max] 范围内的值。
 */
class slider : public widget {
public:
    slider() {
        widget_style.cursor = cursor_type::pointer;
        focusable = true;
    }
    float value = 0.0f;
    float min_value = 0.0f;
    float max_value = 100.0f;
    std::function<void(float)> on_changed;

    bool hit_test(float x, float y) const override;

    void handle_event(const event_type& type, void* data) override;

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;

    float track_thickness = 4.0f;
    float thumb_radius = 8.0f;

private:
    bool dragging_ = false;
    bool thumb_hovered_ = false;

    float value_to_x() const;
    float x_to_value(float px) const;
    void on_hover_change(bool hovered) override;
};

}
