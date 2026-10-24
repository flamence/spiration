/**
 * @file container.cpp
 * @brief 容器控件实现。
 * @author clk
 */

#include <spiration/ui/container.h>
#include <algorithm>

namespace spiration {

void container::layout() {
    on_layout_begin();
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
            // arrange 只负责几何；子控件内部布局统一在此递归一次。
            widget::layout();
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
                // 先按最终宽度布局，再取高度，保证同一帧内滚动范围不回退一帧。
                if (child->needs_layout()) child->layout();
                pref = child->layout_preferred_size();
                child->height = pref.height > 0.0f ? pref.height : child->height;
                child->finish_layout_geometry();
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
        if (!child->visible || child->width <= 0.0f || child->height <= 0.0f) continue;
        // O(1) 可见性判断：由父级下发裁剪区，替代旧实现逐层回溯祖先链的
        // widget::visible_rect()（后者每个子控件 O(深度)，整帧 O(控件数 × 深度)）。
        float cl, ct, cr, cb;
        if (!compute_child_paint_clip(*child, cl, ct, cr, cb)) continue;
        child->set_paint_clip(cl, ct, cr, cb);
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

    bool over_v = has_v_scroll() &&
                  mx >= width - scroll_bar_width && mx <= width &&
                  my >= scroll_bar_padding &&
                  my <= (has_h_scroll() ? height - scroll_bar_width : height) - scroll_bar_padding;
    bool over_h = has_h_scroll() &&
                  my >= height - scroll_bar_width && my <= height &&
                  mx >= scroll_bar_padding &&
                  mx <= (has_v_scroll() ? width - scroll_bar_width : width) - scroll_bar_padding;

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

        bool in_bounds = w_orig.x >= 0.0f && w_orig.x <= width &&
                         w_orig.y >= 0.0f && w_orig.y <= height;
        if (!in_bounds) return;

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
        float track = std::max(1.0f, v_track_h() - v_thumb_height());
        scroll_y_ = drag_start_v_ + (my - drag_start_y_) / track * range;
        scroll_y_ = std::max(0.0f, std::min(scroll_y_, scroll_max_y_));
        md->consumed = true;
        if (request_repaint_) request_repaint_();
        return;
    }
    if (md->action == mouse_action::move && dragging_h_) {
        float range = scroll_max_x_;
        float track = std::max(1.0f, h_track_w() - h_thumb_width());
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
    const bool has_v = has_v_scroll();
    const bool has_h = has_h_scroll();
    if (!has_v && !has_h) return;

    const float pad = scroll_bar_padding;
    const color track_col = {0.12f, 0.12f, 0.12f, 0.7f};

    if (has_v) {
        const float sb_x = width - scroll_bar_width;
        const float track_h = v_track_h();
        renderer->draw_rectangle({sb_x, pad, scroll_bar_width, track_h}, track_col);
        const float thumb_h = std::min(v_thumb_height(), track_h);
        const float inset = pad * 0.5f;
        renderer->draw_rectangle({sb_x + inset, pad + v_thumb_y(),
                                  scroll_bar_width - inset * 2.0f, thumb_h},
                                 thumb_v_bg_.current());
    }
    if (has_h) {
        const float sb_y = height - scroll_bar_width;
        const float track_w = h_track_w();
        renderer->draw_rectangle({pad, sb_y, track_w, scroll_bar_width}, track_col);
        const float thumb_w = std::min(h_thumb_width(), track_w);
        const float inset = pad * 0.5f;
        renderer->draw_rectangle({pad + h_thumb_x(), sb_y + inset,
                                  thumb_w, scroll_bar_width - inset * 2.0f},
                                 thumb_h_bg_.current());
    }
    if (has_v && has_h) {
        // 双滚动条同时出现时填充重叠角落。
        renderer->draw_rectangle({width - scroll_bar_width, height - scroll_bar_width,
                                  scroll_bar_width, scroll_bar_width}, track_col);
    }
}

float container::v_track_h() const {
    return (has_h_scroll() ? height - scroll_bar_width : height) - scroll_bar_padding * 2.0f;
}

float container::h_track_w() const {
    return (has_v_scroll() ? width - scroll_bar_width : width) - scroll_bar_padding * 2.0f;
}

bool container::is_over_scrollbar(float x, float y) const {
    if (has_v_scroll() &&
        x >= width - scroll_bar_width && x <= width &&
        y >= scroll_bar_padding &&
        y <= (has_h_scroll() ? height - scroll_bar_width : height) - scroll_bar_padding)
        return true;
    if (has_h_scroll() &&
        y >= height - scroll_bar_width && y <= height &&
        x >= scroll_bar_padding &&
        x <= (has_v_scroll() ? width - scroll_bar_width : width) - scroll_bar_padding)
        return true;
    return false;
}

widget* container::hit_test_hover(float x, float y) const {
    if (!enabled) return nullptr;
    if (x < 0.0f || x > width || y < 0.0f || y > height) return nullptr;
    if (is_over_scrollbar(x, y)) return const_cast<container*>(this);
    return widget::hit_test_hover(x, y);
}

cursor_style container::effective_cursor(float lx, float ly) const {
    if (is_over_scrollbar(lx, ly)) return cursor_style::default_cursor;
    return widget::effective_cursor(lx, ly);
}

float container::v_thumb_height() const {
    if (content_height_ <= 0.0f) return v_track_h();
    const float track_h = v_track_h();
    const float ratio = std::min(1.0f, height / content_height_);
    return std::min(track_h, std::max(4.0f, track_h * ratio));
}

float container::v_thumb_y() const {
    if (scroll_max_y_ <= 0.0f) return 0.0f;
    const float track_h = v_track_h();
    return (scroll_y_ / scroll_max_y_) * std::max(0.0f, track_h - v_thumb_height());
}

float container::h_thumb_width() const {
    if (content_width_ <= 0.0f) return h_track_w();
    const float track_w = h_track_w();
    const float ratio = std::min(1.0f, width / content_width_);
    return std::min(track_w, std::max(4.0f, track_w * ratio));
}

float container::h_thumb_x() const {
    if (scroll_max_x_ <= 0.0f) return 0.0f;
    const float track_w = h_track_w();
    return (scroll_x_ / scroll_max_x_) * std::max(0.0f, track_w - h_thumb_width());
}

} // namespace spiration
