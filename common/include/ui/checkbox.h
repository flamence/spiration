/**
 * @file checkbox.h
 * @brief 复选框控件，支持选中/未选中状态切换。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme_manager.h>
#include <utils/animation.h>
#include <functional>
#include <string>

namespace spiration {

/**
 * @brief 可交互的复选框控件，带文本标签。
 */
class checkbox : public widget {
public:
    std::string text;
    bool checked = false;
    std::function<void(bool)> on_changed;

    bool hit_test(float x, float y) const override;

    void tick(float dt_ms) override;
    void on_hover_change(bool hovered) override;

    void handle_event(const event_type& type, void* data) override;

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;

    float box_size = 16.0f;
    float gap = 6.0f;

    color hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);

private:
    color_transition bg_transition_{color::transparent()};
};

}
