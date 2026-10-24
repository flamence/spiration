/**
 * @file widget.cpp
 * @brief 控件基类实现。
 * @author clk
 */

#include <spiration/ui/widget.h>
#include <spiration/ui/context_menu.h>
#include <spiration/ui/focus_manager.h>
#include <spiration/ui/root.h>

#include <algorithm>

namespace spiration {

// 绘制轮次号从 1 开始；0 保留给“无裁剪上下文”。
uint64_t widget::paint_pass_ = 1;

bool widget::compute_child_paint_clip(const widget& child, float& l, float& t,
                                      float& r, float& b) const {
    // 子控件在父坐标系中的矩形。
    float cl = child.x;
    float ct = child.y;
    float cr = child.x + child.width;
    float cb = child.y + child.height;

    // 滚动容器的子控件坐标 = 本控件可视坐标 - 滚动偏移，
    // 因此本控件可视区（自身矩形 / 继承裁剪区）需平移到子控件所在的内容坐标系。
    const float sx = scroll_offset_x_for_children();
    const float sy = scroll_offset_for_children();

    // 与本控件矩形求交（与旧 visible_rect() 的祖先边界裁剪语义保持一致）。
    // 本控件尚未布局（宽高未知）时不做该约束，避免首个绘制帧误裁子树。
    if (width > 0.0f && height > 0.0f) {
        cl = std::max(cl, sx);
        ct = std::max(ct, sy);
        cr = std::min(cr, width + sx);
        cb = std::min(cb, height + sy);
    }

    // 与本控件从父级继承的裁剪区求交（同样平移到内容坐标系）。
    if (has_valid_paint_clip()) {
        cl = std::max(cl, paint_clip_l_ + sx);
        ct = std::max(ct, paint_clip_t_ + sy);
        cr = std::min(cr, paint_clip_r_ + sx);
        cb = std::min(cb, paint_clip_b_ + sy);
    }

    if (cr <= cl || cb <= ct) return false;

    // 转换为子控件局部坐标。
    l = cl - child.x;
    t = ct - child.y;
    r = cr - child.x;
    b = cb - child.y;
    return true;
}

void widget::notify_destroyed() {
    focus_manager::instance().on_widget_destroyed(this);
    for (widget* p = parent_; p; p = p->parent()) {
        if (auto* r = dynamic_cast<root*>(p)) {
            r->on_widget_destroyed(this);
            break;
        }
    }
}

void widget::set_visible(bool v) {
    if (visible == v) return;
    visible = v;
    if (!v && focus_manager::instance().focused() == this) {
        focus_manager::instance().clear_focus();
    }
    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

void widget::set_enabled(bool v) {
    if (enabled == v) return;
    enabled = v;
    if (!v && focus_manager::instance().focused() == this) {
        focus_manager::instance().clear_focus();
    }
    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

void widget::notify_destroyed_recursive() {
    notify_destroyed();
    for (auto& child : children_) {
        child->notify_destroyed_recursive();
    }
}

void widget::set_context_menu_callback(std::function<void(float x, float y, std::unique_ptr<context_menu>)> cb) {
    context_menu_cb_ = std::move(cb);
    for (auto& child : children_) {
        child->set_context_menu_callback(context_menu_cb_);
    }
}

void widget::request_context_menu(float x, float y, std::unique_ptr<context_menu> menu) {
    for (widget* w = this; w; w = w->parent_) {
        if (w->context_menu_cb_) {
            w->context_menu_cb_(x, y, std::move(menu));
            return;
        }
    }
}

void widget::relayout_chain(unsigned max_depth) {
    widget* w = this;
    unsigned depth = 0;
    while (w && depth <= max_depth) {
        w->layout();
        w = w->parent_;
        ++depth;
    }
}

root* widget::find_root() {
    for (widget* w = parent_; w; w = w->parent_) {
        if (auto* r = dynamic_cast<root*>(w)) return r;
    }
    return nullptr;
}

} // namespace spiration
