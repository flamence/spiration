/**
 * @file split_pane.cpp
 * @brief 可拖拽分割面板实现�?
 * @author clk
 */

#include <ui/split_pane.h>
#include <algorithm>

namespace spiration {

void split_pane::init() {
    widget_style.background_color = color::transparent();
}

void split_pane::layout() {
    if (children().size() < 2) return;

    if (dir == direction::vertical) {
        float avail = height - handle_size;
        float h1 = avail * split_ratio_;
        float h2 = avail - h1;
        children()[0]->x = 0.0f; children()[0]->y = 0.0f;
        children()[0]->width = width; children()[0]->height = h1;
        children()[0]->layout();
        children()[1]->x = 0.0f; children()[1]->y = h1 + handle_size;
        children()[1]->width = width; children()[1]->height = h2;
        children()[1]->layout();
    } else {
        float avail = width - handle_size;
        float w1 = avail * split_ratio_;
        float w2 = avail - w1;
        children()[0]->x = 0.0f; children()[0]->y = 0.0f;
        children()[0]->width = w1; children()[0]->height = height;
        children()[0]->layout();
        children()[1]->x = w1 + handle_size; children()[1]->y = 0.0f;
        children()[1]->width = w2; children()[1]->height = height;
        children()[1]->layout();
    }
}

void split_pane::paint(std::shared_ptr<renderer> renderer) {
    widget::paint(renderer);

    color hc = (hovering_handle_ || dragging_)
                   ? theme_manager::get(theme_manager::SPLIT_HANDLE_HOVER)
                   : theme_manager::get(theme_manager::SPLIT_HANDLE);

    if (dir == direction::vertical) {
        float hy = height * split_ratio_;
        renderer->draw_rectangle({x, y + hy, width, handle_size}, hc);
    } else {
        float hx = width * split_ratio_;
        renderer->draw_rectangle({x + hx, y, handle_size, height}, hc);
    }
}

void split_pane::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        if (dir == direction::vertical) {
            float hy = height * split_ratio_;
            bool near = (md->position.y >= hy - 4.0f && md->position.y <= hy + handle_size + 4.0f);
            hovering_handle_ = near;

            if (md->action == mouse_action::down && near) {
                dragging_ = true;
                drag_start_pos_ = md->position.y;
                drag_start_ratio_ = split_ratio_;
                md->consumed = true;
                return;
            }
            if (md->action == mouse_action::move && dragging_) {
                float dy = md->position.y - drag_start_pos_;
                float new_r = drag_start_ratio_ + dy / (height - handle_size);
                split_ratio_ = std::max(min_ratio, std::min(max_ratio, new_r));
                layout();
                md->consumed = true;
                if (request_repaint_) request_repaint_();
                return;
            }
        } else {
            float hx = width * split_ratio_;
            bool near = (md->position.x >= hx - 4.0f && md->position.x <= hx + handle_size + 4.0f);
            hovering_handle_ = near;

            if (md->action == mouse_action::down && near) {
                dragging_ = true;
                drag_start_pos_ = md->position.x;
                drag_start_ratio_ = split_ratio_;
                md->consumed = true;
                return;
            }
            if (md->action == mouse_action::move && dragging_) {
                float dx = md->position.x - drag_start_pos_;
                float new_r = drag_start_ratio_ + dx / (width - handle_size);
                split_ratio_ = std::max(min_ratio, std::min(max_ratio, new_r));
                layout();
                md->consumed = true;
                if (request_repaint_) request_repaint_();
                return;
            }
        }

        if (md->action == mouse_action::up) dragging_ = false;
    }

    if (!dragging_) container::handle_event(type, data);
}

void split_pane::set_split_ratio(float ratio) {
    split_ratio_ = std::max(min_ratio, std::min(max_ratio, ratio));
    layout();
    if (request_repaint_) request_repaint_();
}

widget* split_pane::first() const {
    return children().empty() ? nullptr : children()[0].get();
}

widget* split_pane::second() const {
    return children().size() < 2 ? nullptr : children()[1].get();
}

}
