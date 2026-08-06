/**
 * @file split_pane.cpp
 * @brief 可拖拽分割面板实现。
 * @author clk
 */

#include <ui/split_pane.h>
#include <ui/focus_manager.h>
#include <algorithm>

namespace spiration {

void split_pane::init() {
    widget_style.background_color = color::transparent();
}

void split_pane::layout() {
    if (!auto_layout) { widget::layout(); return; }
    if (children().size() < 2) return;
    on_layout_begin();

    if (dir == direction::vertical) {
        float avail = height - handle_size;
        float h1 = avail * split_ratio_;
        float h2 = avail - h1;
        children()[0]->x = 0.0f; children()[0]->y = 0.0f;
        children()[0]->width = width; children()[0]->height = h1;
        if (children()[0]->needs_layout()) children()[0]->layout();
        children()[1]->x = 0.0f; children()[1]->y = h1 + handle_size;
        children()[1]->width = width; children()[1]->height = h2;
        if (children()[1]->needs_layout()) children()[1]->layout();
    } else {
        float avail = width - handle_size;
        float w1 = avail * split_ratio_;
        float w2 = avail - w1;
        children()[0]->x = 0.0f; children()[0]->y = 0.0f;
        children()[0]->width = w1; children()[0]->height = height;
        if (children()[0]->needs_layout()) children()[0]->layout();
        children()[1]->x = w1 + handle_size; children()[1]->y = 0.0f;
        children()[1]->width = w2; children()[1]->height = height;
        if (children()[1]->needs_layout()) children()[1]->layout();
    }
}

void split_pane::paint(std::shared_ptr<renderer> renderer) {
    widget::paint(renderer);
    if (!show_handle) return;

    color hc = (hovering_handle_ || dragging_)
                   ? theme_manager::get(theme_manager::SPLIT_HANDLE_HOVER)
                   : theme_manager::get(theme_manager::SPLIT_HANDLE);

    if (dir == direction::vertical) {
        float hy = height * split_ratio_;
        renderer->draw_rectangle({0, hy, width, handle_size}, hc);
    } else {
        float hx = width * split_ratio_;
        renderer->draw_rectangle({hx, 0, handle_size, height}, hc);
    }
}

bool split_pane::handle_hit(float pos) const {
    const float h = (dir == direction::vertical) ? height * split_ratio_
                                                 : width * split_ratio_;
    const float tol = 2.0f;
    return pos >= h - tol && pos <= h + handle_size + tol;
}

/// @brief 按像素约束钳制拖拽比例。
float split_pane::clamp_ratio_for_drag(float r) const {
    float len = (dir == direction::vertical) ? height : width;
    float avail = len - handle_size;
    if (avail <= 0.0f) return r;
    float w1 = avail * r;
    if (min_first_px > 0.0f) w1 = std::max(w1, min_first_px);
    if (max_first_px > 0.0f) w1 = std::min(w1, max_first_px);
    if (min_second_px > 0.0f) w1 = std::min(w1, avail - min_second_px);
    return w1 / avail;
}

widget* split_pane::hit_test_hover(float x, float y) const {
    if (!enabled) return nullptr;
    if (x < 0.0f || x > width || y < 0.0f || y > height) return nullptr;
    if (show_handle) {
        const float pos = (dir == direction::vertical) ? y : x;
        if (handle_hit(pos)) return const_cast<split_pane*>(this);
    }
    return widget::hit_test_hover(x, y);
}

cursor_type split_pane::effective_cursor(float lx, float ly) const {
    if (show_handle) {
        const float pos = (dir == direction::vertical) ? ly : lx;
        if (dragging_ || handle_hit(pos)) {
            return (dir == direction::vertical) ? cursor_type::resize_v
                                                : cursor_type::resize_h;
        }
    }
    return widget::effective_cursor(lx, ly);
}

void split_pane::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        if (show_handle) {
            if (dir == direction::vertical) {
                hovering_handle_ = handle_hit(md->position.y);

                if (md->action == mouse_action::down && hovering_handle_) {
                    dragging_ = true;
                    drag_start_pos_ = md->position.y;
                    drag_start_ratio_ = split_ratio_;
                    md->consumed = true;
                    focus_manager::instance().clear_focus();
                    return;
                }
                if (md->action == mouse_action::move && dragging_) {
                    float dy = md->position.y - drag_start_pos_;
                    float new_r = drag_start_ratio_ + dy / (height - handle_size);
                    split_ratio_ = clamp_ratio_for_drag(
                        std::max(min_ratio, std::min(max_ratio, new_r)));
                    layout();
                    md->consumed = true;
                    if (on_ratio_changed) on_ratio_changed();
                    if (request_repaint_) request_repaint_();
                    return;
                }
            } else {
                hovering_handle_ = handle_hit(md->position.x);

                if (md->action == mouse_action::down && hovering_handle_) {
                    dragging_ = true;
                    drag_start_pos_ = md->position.x;
                    drag_start_ratio_ = split_ratio_;
                    md->consumed = true;
                    focus_manager::instance().clear_focus();
                    return;
                }
                if (md->action == mouse_action::move && dragging_) {
                    float dx = md->position.x - drag_start_pos_;
                    float new_r = drag_start_ratio_ + dx / (width - handle_size);
                    split_ratio_ = clamp_ratio_for_drag(
                        std::max(min_ratio, std::min(max_ratio, new_r)));
                    layout();
                    md->consumed = true;
                    if (on_ratio_changed) on_ratio_changed();
                    if (request_repaint_) request_repaint_();
                    return;
                }
            }

            if (md->action == mouse_action::up) dragging_ = false;
        }
    }

    if (!dragging_) container::handle_event(type, data);
}

void split_pane::set_split_ratio(float ratio) {
    split_ratio_ = std::max(min_ratio, std::min(max_ratio, ratio));
    layout();
    if (on_ratio_changed) on_ratio_changed();
    if (request_repaint_) request_repaint_();
}

widget* split_pane::first() const {
    return children().empty() ? nullptr : children()[0].get();
}

widget* split_pane::second() const {
    return children().size() < 2 ? nullptr : children()[1].get();
}

} // namespace spiration
