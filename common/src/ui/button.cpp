/**
 * @file button.cpp
 * @brief 按钮控件实现，支持背景色平滑过渡动画。
 * @author clk
 */

#include <ui/button.h>
#include <ui/theme.h>
#include <utils/console.h>

namespace spiration {

bool button::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void button::tick(float dt_ms) {
    if (bg_transition_.update(dt_ms)) {
        if (request_repaint_) request_repaint_();
    }
    container::tick(dt_ms);
}

void button::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* mouse_data = static_cast<mouse_event_data*>(data);
        bool old_hover = hovering_;
        bool old_press = pressing_;

        
        hovering_ = (mouse_data->position.x >= 0.0f && mouse_data->position.x <= width &&
                     mouse_data->position.y >= 0.0f && mouse_data->position.y <= height);

        if (mouse_data->action == mouse_action::down && hovering_) {
            pressing_ = true;
            mouse_data->consumed = true;
        } else if (mouse_data->action == mouse_action::up) {
            pressing_ = false;
        }

        
        if (hovering_ != old_hover || pressing_ != old_press) {
            color target;
            if (hovering_ && pressing_)      target = press_color;
            else if (hovering_)              target = hover_color;
            else                             target = color::transparent();
            bg_transition_.animate_to(target, 120.0f);
            if (request_repaint_) request_repaint_();
        }
    }

    container::handle_event(type, data);
}

void button::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({x, y, width, height}, bg_transition_.current());
    renderer->draw_text_aligned(text, rectangle{ x, y, width, height }, theme::button_text(), 
                                text_alignment::center, vertical_alignment::center, 16.0f);
}

}