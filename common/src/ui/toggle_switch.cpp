/**
 * @file toggle_switch.cpp
 * @brief 开关控件实现，滑块带平滑过渡动画�?
 * @author clk
 */

#include <ui/toggle_switch.h>

namespace spiration {

bool toggle_switch::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void toggle_switch::tick(float dt_ms) {
    if (knob_pos_.update(dt_ms)) {
        if (request_repaint_) request_repaint_();
    }
    widget::tick(dt_ms);
}

void toggle_switch::on_hover_change(bool) {}

void toggle_switch::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        if (md->action == mouse_action::down && is_hovered()) {
            md->consumed = true;
            set_active(!active);
            if (on_changed) on_changed(active);
            if (request_repaint_) request_repaint_();
        }
    }
    widget::handle_event(type, data);
}

void toggle_switch::paint(std::shared_ptr<renderer> renderer) {
    float cy = y + height * 0.5f;
    float tx = x;
    float ty = cy - track_height * 0.5f;
    float t = knob_pos_.current();

    color track_bg = theme_manager::get(theme_manager::TOGGLE_BG);
    color track_active = theme_manager::get(theme_manager::TOGGLE_BG_ACTIVE);
    color track_color;
    track_color.r = track_bg.r + (track_active.r - track_bg.r) * t;
    track_color.g = track_bg.g + (track_active.g - track_bg.g) * t;
    track_color.b = track_bg.b + (track_active.b - track_bg.b) * t;
    track_color.a = track_bg.a + (track_active.a - track_bg.a) * t;
    renderer->draw_rounded_rectangle({tx, ty, track_width, track_height}, track_color, track_height * 0.5f);

    float knob_diameter = track_height - knob_margin * 2.0f;
    float travel = track_width - knob_diameter - knob_margin * 2.0f;
    float kx = tx + knob_margin + travel * t;
    float ky = cy - knob_diameter * 0.5f;
    renderer->draw_rounded_rectangle({kx, ky, knob_diameter, knob_diameter}, theme_manager::get(theme_manager::TOGGLE_KNOB), knob_diameter * 0.5f);
}

size toggle_switch::layout_preferred_size() const {
    return {track_width, track_height};
}

}
