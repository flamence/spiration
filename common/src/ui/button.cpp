/**
 * @file button.cpp
 * @brief 按钮控件实现。
 * @author clk
 */

#include <ui/button.h>
#include <ui/theme_manager.h>
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

void button::on_hover_change(bool hovered) {
    if (!hovered) pressing_ = false;
    color target;
    if (hovered && pressing_)      target = press_color;
    else if (hovered)              target = hover_color;
    else                           target = color::transparent();
    bg_transition_.animate_to(target, 120.0f);
}

void button::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* mouse_data = static_cast<mouse_event_data*>(data);

        if (mouse_data->action == mouse_action::down && is_hovered()) {
            pressing_ = true;
            mouse_data->consumed = true;
            color target = press_color;
            bg_transition_.animate_to(target, 60.0f);
        } else if (mouse_data->action == mouse_action::up && pressing_) {
            pressing_ = false;
            color target = is_hovered() ? hover_color : color::transparent();
            bg_transition_.animate_to(target, 80.0f);
        }
    }

    container::handle_event(type, data);
}

void button::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({x, y, width, height}, bg_transition_.current());
    renderer->draw_text_aligned(text, rectangle{ x, y, width, height }, theme_manager::get(theme_manager::BUTTON_TEXT), 
                                text_alignment::center, vertical_alignment::center, 16.0f);
}

}