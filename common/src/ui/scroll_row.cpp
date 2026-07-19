/**
 * @file scroll_row.cpp
 * @brief 可水平滚动的行容器实现。
 * @author clk
 */

#include <ui/scroll_row.h>
#include <algorithm>
#include <cmath>

namespace spiration {

void scroll_row::init() {
    widget_style.background_color = color::transparent();
}

void scroll_row::layout() {
    content_width_ = 0.0f;
    if (child_width_ > 0.0f) {
        content_width_ = static_cast<float>(children().size()) * child_width_;
        float cx = 0.0f;
        for (auto& child : children()) {
            child->x = cx;
            child->y = 0.0f;
            child->width = child_width_;
            child->height = height;
            child->layout();
            cx += child_width_;
        }
    }
    scroll_max_ = std::max(0.0f, content_width_ - width);
    if (scroll_offset_ > scroll_max_) scroll_offset_ = scroll_max_;
}

void scroll_row::paint(std::shared_ptr<renderer> renderer) {
    float vis_start = scroll_offset_;
    float vis_end = scroll_offset_ + width;
    float cx = 0.0f;
    renderer->push_transform(x - scroll_offset_, y);
    for (auto& child : children()) {
        float child_end = cx + child->width;
        if (child_end > vis_start && cx < vis_end) {
            child->paint(renderer);
        }
        cx += child_width_;
    }
    renderer->pop_transform();
}

void scroll_row::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        point original = md->position;

        if (md->action == mouse_action::wheel) {
            float step = (md->wheel_delta > 0) ? -30.0f : 30.0f;
            scroll_offset_ += step;
            scroll_offset_ = std::max(0.0f, std::min(scroll_offset_, scroll_max_));
            md->consumed = true;
            if (request_repaint_) request_repaint_();
            return;
        }

        for (auto& child : children()) {
            md->position.x = original.x + scroll_offset_ - child->x;
            md->position.y = original.y - child->y;
            child->handle_event(type, data);
            if (md->consumed) {
                md->position = original;
                return;
            }
        }
        md->position = original;
        return;
    }

    container::handle_event(type, data);
}

} 
