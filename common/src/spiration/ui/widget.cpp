/**
 * @file widget.cpp
 * @brief 控件基类实现。
 * @author clk
 */

#include <spiration/ui/widget.h>
#include <spiration/ui/context_menu.h>
#include <spiration/ui/focus_manager.h>
#include <spiration/ui/root.h>

namespace spiration {

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
