/**
 * @file scroll_view.cpp
 * @brief 垂直滚动容器实现�?
 * @author clk
 */

#include <ui/scroll_view.h>
#include <algorithm>

namespace spiration {

void scroll_view::init() {
    widget_style.background_color = color::transparent();
}

void scroll_view::layout() {
    float avail = width - scroll_bar_width;
    content_height_ = 0.0f;
    for (auto& child : children()) {
        child->x = 0.0f;
        child->y = content_height_;
        child->width = avail;
        size pref = child->layout_preferred_size();
        child->height = pref.height > 0.0f ? pref.height : child->height;
        child->layout();
        content_height_ += child->height;
    }
    scroll_max_ = std::max(0.0f, content_height_ - height);
    scroll_offset_ = std::max(0.0f, std::min(scroll_offset_, scroll_max_));
}

void scroll_view::paint(std::shared_ptr<renderer> renderer) {
    renderer->push_clip({x, y, width, height});
    renderer->push_transform(x, y - scroll_offset_);
    for (auto& child : children()) {
        child->paint(renderer);
    }
    renderer->pop_transform();
    renderer->pop_clip();

    if (scroll_max_ > 0.0f) {
        float sb_x = x + width - scroll_bar_width;
        renderer->draw_rectangle({sb_x, y, scroll_bar_width, height},
                                 theme_manager::get(theme_manager::SCROLL_BAR_BG));

        float th = thumb_height();
        float ty = y + thumb_y();
        color thumb_c = (hovering_scrollbar_ || dragging_scrollbar_)
                            ? theme_manager::get(theme_manager::SCROLL_BAR_THUMB_HOVER)
                            : theme_manager::get(theme_manager::SCROLL_BAR_THUMB);
        renderer->draw_rounded_rectangle({sb_x + 2.0f, ty, scroll_bar_width - 4.0f, th},
                                         thumb_c, 3.0f);
    }
}

void scroll_view::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        bool over_sb = (mx >= width - scroll_bar_width && mx <= width);
        hovering_scrollbar_ = over_sb && scroll_max_ > 0.0f;

        if (md->action == mouse_action::wheel) {
            float step = (md->wheel_delta > 0) ? -40.0f : 40.0f;
            scroll_offset_ = std::max(0.0f, std::min(scroll_offset_ + step, scroll_max_));
            md->consumed = true;
            if (request_repaint_) request_repaint_();
            return;
        }

        if (md->action == mouse_action::down && hovering_scrollbar_) {
            dragging_scrollbar_ = true;
            drag_start_y_ = my;
            drag_start_offset_ = scroll_offset_;
            md->consumed = true;
            return;
        }

        if (md->action == mouse_action::up) {
            dragging_scrollbar_ = false;
        }

        if (md->action == mouse_action::move && dragging_scrollbar_) {
            float ratio = scroll_max_ / (height - thumb_height());
            scroll_offset_ = drag_start_offset_ + (my - drag_start_y_) * ratio;
            scroll_offset_ = std::max(0.0f, std::min(scroll_offset_, scroll_max_));
            md->consumed = true;
            if (request_repaint_) request_repaint_();
            return;
        }

        point orig = md->position;
        md->position.x = orig.x;
        md->position.y = orig.y + scroll_offset_;
        container::handle_event(type, data);
        md->position = orig;
        return;
    }
    container::handle_event(type, data);
}

void scroll_view::scroll_to(float y) {
    scroll_offset_ = std::max(0.0f, std::min(y, scroll_max_));
    if (request_repaint_) request_repaint_();
}

float scroll_view::thumb_height() const {
    if (content_height_ <= 0.0f) return height;
    float ratio = height / content_height_;
    return std::max(20.0f, height * ratio);
}

float scroll_view::thumb_y() const {
    if (scroll_max_ <= 0.0f) return 0.0f;
    return (scroll_offset_ / scroll_max_) * (height - thumb_height());
}

}
