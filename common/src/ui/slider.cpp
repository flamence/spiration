/**
 * @file slider.cpp
 * @brief 滑块控件实现。
 * @author clk
 */

#include <ui/slider.h>

namespace spiration {

float slider::value_to_x() const {
    float range = max_value - min_value;
    if (range <= 0.0f) return thumb_radius;
    float t = (value - min_value) / range;
    float usable = width - thumb_radius * 2.0f;
    return thumb_radius + t * usable;
}

float slider::x_to_value(float px) const {
    float usable = width - thumb_radius * 2.0f;
    if (usable <= 0.0f) return min_value;
    float t = (px - thumb_radius) / usable;
    t = std::max(0.0f, std::min(1.0f, t));
    return min_value + t * (max_value - min_value);
}

bool slider::hit_test(float x, float y) const {
    float vx = value_to_x();
    float cy = height * 0.5f;
    float dx = x - vx;
    float dy = y - cy;
    return (dx * dx + dy * dy) <= (thumb_radius * 2.0f) * (thumb_radius * 2.0f);
}

void slider::on_hover_change(bool hovered) {
    if (!hovered) { dragging_ = false; thumb_hovered_ = false; }
}

void slider::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;
        float cy = height * 0.5f;
        float vx = value_to_x();

        thumb_hovered_ = is_hovered() &&
            (std::abs(my - cy) < thumb_radius * 2.5f) &&
            (std::abs(mx - vx) < thumb_radius * 2.0f);

        bool near_thumb = thumb_hovered_;

        if (md->action == mouse_action::down) {
            if (near_thumb || (my >= cy - track_thickness * 3.0f && my <= cy + track_thickness * 3.0f)) {
                dragging_ = true;
                md->consumed = true;
                value = x_to_value(mx);
                value = std::max(min_value, std::min(max_value, value));
                if (on_changed) on_changed(value);
                if (request_repaint_) request_repaint_();
            }
        } else if (md->action == mouse_action::up) {
            dragging_ = false;
        } else if (md->action == mouse_action::move && dragging_) {
            md->consumed = true;
            value = x_to_value(mx);
            value = std::max(min_value, std::min(max_value, value));
            if (on_changed) on_changed(value);
            if (request_repaint_) request_repaint_();
        }
    }
    widget::handle_event(type, data);
}

void slider::paint(std::shared_ptr<renderer> renderer) {
    float cy = height * 0.5f;

    renderer->draw_rectangle(
        {0, cy - track_thickness * 0.5f, width, track_thickness},
        theme_manager::get(theme_manager::SLIDER_TRACK));

    float localVx = value_to_x();

    renderer->draw_rectangle(
        {0, cy - track_thickness * 0.5f, localVx, track_thickness},
        theme_manager::get(theme_manager::SLIDER_FILL));

    color thumb_c = thumb_hovered_ || dragging_
                        ? theme_manager::get(theme_manager::SLIDER_THUMB_HOVER)
                        : theme_manager::get(theme_manager::SLIDER_THUMB);
    renderer->draw_rectangle(
        {localVx - thumb_radius, cy - thumb_radius, thumb_radius * 2.0f, thumb_radius * 2.0f},
        thumb_c);
}

size slider::layout_preferred_size() const {
    return {width, thumb_radius * 4.0f};
}

} // namespace spiration
