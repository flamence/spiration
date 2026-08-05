/**
 * @file radio_button.h
 * @brief 单选按钮控件，支持分组互斥。
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
 * @brief 单选按钮，带文本标签，支持 `on_changed` 回调。
 */
class radio_button : public widget {
public:
    radio_button() {
        widget_style.cursor = cursor_type::pointer;
        focusable = true;
    }
    std::string text;
    bool selected = false;
    std::function<void(bool)> on_changed;
    int group_id = 0;

    bool hit_test(float x, float y) const override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;

    float radius = 8.0f;
    float gap = 6.0f;

private:
    bool hovering_ = false;
};

}
