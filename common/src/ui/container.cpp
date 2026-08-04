/**
 * @file container.cpp
 * @brief 容器控件实现。
 * @author clk
 */

#include <ui/container.h>
#include <algorithm>

namespace spiration {

void container::layout() {
    bool scroll_v = widget_style.overflow_y;
    bool scroll_h = widget_style.overflow_x;

    if (scroll_v || scroll_h) {
        const auto& p = widget_style.padding;
        float pl = static_cast<float>(p.left);
        float pt = static_cast<float>(p.top);
        float pr = static_cast<float>(p.right);
        float pb = static_cast<float>(p.bottom);

        if (layout_manager_) {
            size pref = layout_manager_->preferred_size(children());
            float avail_w = std::max(0.0f, width - pl - pr);
            float avail_h = std::max(0.0f, height - pt - pb);
            float content_w = std::max(pref.width, avail_w);
            float content_h = std::max(pref.height, avail_h);
            content_width_ = content_w + pl + pr;
            content_height_ = content_h + pt + pb;
            layout_manager_->arrange({pl, pt, content_w, content_h}, children());
        } else {
            float avail_w = std::max(0.0f, width - pl - pr - (scroll_v ? scroll_bar_width : 0.0f));
            content_width_ = 0.0f;
            content_height_ = 0.0f;
            float y = pt;
            for (auto& child : children()) {
                child->x = pl;
                child->y = y;
                size pref = child->layout_preferred_size();
                float child_w = avail_w;
                if (scroll_h) child_w = std::max(avail_w, pref.width);
                child->width = child_w;
                child->height = pref.height > 0.0f ? pref.height : child->height;
                child->layout();
                content_width_ = std::max(content_width_, child_w + pl + pr);
                y += child->height;
            }
            content_height_ = y + pb;
        }

        scroll_max_x_ = std::max(0.0f, content_width_ - width);
        scroll_max_y_ = std::max(0.0f, content_height_ - height);
        scroll_x_ = std::max(0.0f, std::min(scroll_x_, scroll_max_x_));
        scroll_y_ = std::max(0.0f, std::min(scroll_y_, scroll_max_y_));
        return;
    }

    if (layout_manager_) {
        const auto& p = widget_style.padding;
        float pw = std::max(0.0f, width - static_cast<float>(p.left + p.right));
        float ph = std::max(0.0f, height - static_cast<float>(p.top + p.bottom));
        layout_manager_->arrange({static_cast<float>(p.left), static_cast<float>(p.top),
                                  pw, ph}, children());
    }
    widget::layout();
}

size container::layout_preferred_size() const {
    if (layout_manager_) {
        size pref = layout_manager_->preferred_size(children());
        pref.width += static_cast<float>(widget_style.padding.left + widget_style.padding.right);
        pref.height += static_cast<float>(widget_style.padding.top + widget_style.padding.bottom);
        return pref;
    }
    return widget::layout_preferred_size();
}

void container::paint(std::shared_ptr<renderer> renderer) {
    if (widget_style.background_color.a > 0.0f) {
        renderer->draw_rectangle({0, 0, width, height}, widget_style.background_color);
    }

    bool scroll_v = widget_style.overflow_y;
    bool scroll_h = widget_style.overflow_x;

    if (scroll_v || scroll_h) {
        renderer->push_clip({0, 0, width, height});
        renderer->push_transform(-scroll_x_, -scroll_y_);
        paint_children_culled(renderer);
        renderer->pop_transform();
        renderer->pop_clip();

        draw_scrollbars(renderer);
        return;
    }

    paint_children_culled(renderer);
}

void container::paint_children_culled(std::shared_ptr<renderer> renderer) {
    for (auto& child : children()) {
        float vx, vy, vw, vh;
        if (child->width > 0.0f && child->height > 0.0f &&
            !child->visible_rect(vx, vy, vw, vh))
            continue;
        renderer->push_transform(child->x, child->y);
        child->paint(renderer);
        renderer->pop_transform();
    }
}

void container::tick(float dt_ms) {
    bool repaint = false;
    repaint |= thumb_v_bg_.update(dt_ms);
    repaint |= thumb_h_bg_.update(dt_ms);
    if (repaint && request_repaint_) request_repaint_();
    widget::tick(dt_ms);
}

void container::handle_event(const event_type& type, void* data) {
    if (!enabled) {
        widget::handle_event(type, data);
        return;
    }

    bool scroll_v = widget_style.overflow_y;
    bool scroll_h = widget_style.overflow_x;
    if (type != event_type::mouse || (!scroll_v && !scroll_h)) {
        widget::handle_event(type, data);
        return;
    }

    auto* md = static_cast<mouse_event_data*>(data);
    float mx = md->position.x;
    float my = md->position.y;

    bool over_v = scroll_v && scroll_max_y_ > 0.0f &&
                  mx >= width - scroll_bar_width && mx <= width;
    bool over_h = scroll_h && scroll_max_x_ > 0.0f &&
                  my >= height - scroll_bar_width && my <= height;

    if (hovering_v_ != over_v) {
        hovering_v_ = over_v;
        if (over_v) thumb_v_bg_.animate_to({0.55f, 0.55f, 0.55f, 0.8f}, 120.0f);
        else thumb_v_bg_.animate_to({0.45f, 0.45f, 0.45f, 0.5f}, 150.0f);
        if (request_repaint_) request_repaint_();
    }
    if (hovering_h_ != over_h) {
        hovering_h_ = over_h;
        if (over_h) thumb_h_bg_.animate_to({0.55f, 0.55f, 0.55f, 0.8f}, 120.0f);
        else thumb_h_bg_.animate_to({0.45f, 0.45f, 0.45f, 0.5f}, 150.0f);
        if (request_repaint_) request_repaint_();
    }

    if (md->action == mouse_action::wheel) {
        point w_orig = md->position;
        md->position.x = w_orig.x + scroll_x_;
        md->position.y = w_orig.y + scroll_y_;
        const bool child_consumed_before = md->consumed;
        widget::handle_event(type, data);
        md->position = w_orig;
        if (md->consumed && !child_consumed_before) return;

        if (scroll_v && !md->shift) {
            float step = (md->wheel_delta > 0) ? -40.0f : 40.0f;
            float ny = std::max(0.0f, std::min(scroll_y_ + step, scroll_max_y_));
            if (ny != scroll_y_) {
                scroll_y_ = ny;
                md->consumed = true;
                if (request_repaint_) request_repaint_();
            }
            return;
        }
        if (scroll_h) {
            float step = (md->wheel_delta > 0) ? -40.0f : 40.0f;
            float nx = std::max(0.0f, std::min(scroll_x_ + step, scroll_max_x_));
            if (nx != scroll_x_) {
                scroll_x_ = nx;
                md->consumed = true;
                if (request_repaint_) request_repaint_();
            }
            return;
        }
        return;
    }

    if (md->action == mouse_action::down && md->button == mouse_button::left && over_v) {
        dragging_v_ = true;
        drag_start_y_ = my;
        drag_start_v_ = scroll_y_;
        set_mouse_capture(this);
        md->consumed = true;
        return;
    }
    if (md->action == mouse_action::down && md->button == mouse_button::left && over_h) {
        dragging_h_ = true;
        drag_start_x_ = mx;
        drag_start_h_ = scroll_x_;
        set_mouse_capture(this);
        md->consumed = true;
        return;
    }
    if (md->action == mouse_action::up) {
        if (dragging_v_ || dragging_h_) set_mouse_capture(nullptr);
        dragging_v_ = false;
        dragging_h_ = false;
    }
    if (md->action == mouse_action::move && dragging_v_) {
        float range = scroll_max_y_;
        float track = std::max(1.0f, height - v_thumb_height());
        scroll_y_ = drag_start_v_ + (my - drag_start_y_) / track * range;
        scroll_y_ = std::max(0.0f, std::min(scroll_y_, scroll_max_y_));
        md->consumed = true;
        if (request_repaint_) request_repaint_();
        return;
    }
    if (md->action == mouse_action::move && dragging_h_) {
        float range = scroll_max_x_;
        float track = std::max(1.0f, width - h_thumb_width());
        scroll_x_ = drag_start_h_ + (mx - drag_start_x_) / track * range;
        scroll_x_ = std::max(0.0f, std::min(scroll_x_, scroll_max_x_));
        md->consumed = true;
        if (request_repaint_) request_repaint_();
        return;
    }

    point orig = md->position;
    md->position.x = orig.x + scroll_x_;
    md->position.y = orig.y + scroll_y_;
    widget::handle_event(type, data);
    md->position = orig;
}

void container::scroll_to_x(float x) {
    scroll_x_ = std::max(0.0f, std::min(x, scroll_max_x_));
    if (request_repaint_) request_repaint_();
}

void container::scroll_to_y(float y) {
    scroll_y_ = std::max(0.0f, std::min(y, scroll_max_y_));
    if (request_repaint_) request_repaint_();
}

void container::scroll_to(float x, float y) {
    scroll_to_x(x);
    scroll_to_y(y);
}

void container::set_scroll_content(float content_w, float content_h) {
    content_width_ = std::max(0.0f, content_w);
    content_height_ = std::max(0.0f, content_h);
    scroll_max_x_ = std::max(0.0f, content_width_ - width);
    scroll_max_y_ = std::max(0.0f, content_height_ - height);
    scroll_x_ = std::max(0.0f, std::min(scroll_x_, scroll_max_x_));
    scroll_y_ = std::max(0.0f, std::min(scroll_y_, scroll_max_y_));
}

void container::draw_scrollbars(std::shared_ptr<renderer> renderer) {
    bool scroll_v = widget_style.overflow_y;
    bool scroll_h = widget_style.overflow_x;

    if (scroll_v && scroll_max_y_ > 0.0f) {
        float sb_x = width - scroll_bar_width;
        renderer->draw_rectangle({sb_x, 0, scroll_bar_width, height},
                                 {0.12f, 0.12f, 0.12f, 0.7f});
        float th = v_thumb_height();
        float ty = v_thumb_y();
        renderer->draw_rectangle({sb_x, ty, scroll_bar_width, th}, thumb_v_bg_.current());
    }
    if (scroll_h && scroll_max_x_ > 0.0f) {
        float sb_y = height - scroll_bar_width;
        renderer->draw_rectangle({0, sb_y, width, scroll_bar_width},
                                 {0.12f, 0.12f, 0.12f, 0.7f});
        float tw = h_thumb_width();
        float tx = h_thumb_x();
        renderer->draw_rectangle({tx, sb_y, tw, scroll_bar_width}, thumb_h_bg_.current());
    }
}

float container::v_thumb_height() const {
    if (content_height_ <= 0.0f) return height;
    float ratio = height / content_height_;
    return std::max(4.0f, height * ratio);
}

float container::v_thumb_y() const {
    if (scroll_max_y_ <= 0.0f) return 0.0f;
    return (scroll_y_ / scroll_max_y_) * (height - v_thumb_height());
}

float container::h_thumb_width() const {
    if (content_width_ <= 0.0f) return width;
    float ratio = width / content_width_;
    return std::max(4.0f, width * ratio);
}

float container::h_thumb_x() const {
    if (scroll_max_x_ <= 0.0f) return 0.0f;
    return (scroll_x_ / scroll_max_x_) * (width - h_thumb_width());
}

} // namespace spiration