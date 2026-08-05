/**
 * @file button.cpp
 * @brief 按钮控件实现。
 * @author clk
 */

#include <ui/button.h>
#include <ui/focus_manager.h>
#include <ui/theme_manager.h>
#include <ui/text_utils.h>
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
    else                           target = base_bg;
    bg_transition_.animate_to(target, 120.0f);
}

void button::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* mouse_data = static_cast<mouse_event_data*>(data);

        if (mouse_data->action == mouse_action::down && is_hovered() &&
            mouse_data->button == mouse_button::right) {
            mouse_data->consumed = true;
            if (on_right_click)
                on_right_click(mouse_data->position.x, mouse_data->position.y);
            return;
        }

        if (mouse_data->action == mouse_action::down && is_hovered()) {
            pressing_ = true;
            mouse_data->consumed = true;
            focus_manager::instance().request_focus(this);
            bg_transition_.animate_to(press_color, 60.0f);
        } else if (mouse_data->action == mouse_action::up && pressing_) {
            pressing_ = false;
            mouse_data->consumed = true;
            bg_transition_.animate_to(is_hovered() ? hover_color : base_bg, 80.0f);
            if (on_click) {
                on_click();
                return;
            }
        }
    }

    container::handle_event(type, data);
}

void button::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({0, 0, width, height}, bg_transition_.current());
    if (focused_) {
        renderer->draw_rectangle_outline({0, 0, width, height},
                                         theme_manager::get(theme_manager::INPUT_FOCUS_BORDER), 1.0f);
    }
    const auto& p = widget_style.padding;
    std::string display = text;
    if (ellipsize && !display.empty()) {
        float avail = std::max(0.0f, width - p.left - p.right);
        display = text_utils::ellipsize(renderer, display, 16.0f, avail);
    }
    renderer->draw_text_aligned(display, rectangle{ static_cast<float>(p.left), 0,
                                                    std::max(0.0f, width - p.left - p.right), height },
                                theme_manager::get(theme_manager::BUTTON_TEXT),
                                h_align, vertical_alignment::center, 16.0f);
}

}