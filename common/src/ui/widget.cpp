/**
 * @file widget.cpp
 * @brief 控件基类实现。
 * @author clk
 */

#include <ui/widget.h>
#include <ui/context_menu.h>
#include <ui/focus_manager.h>
#include <ui/root.h>

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

void widget::relayout_chain() {
    widget* w = this;
    while (w) {
        w->layout();
        w = w->parent_;
    }
}

root* widget::find_root() {
    for (widget* w = parent_; w; w = w->parent_) {
        if (auto* r = dynamic_cast<root*>(w)) return r;
    }
    return nullptr;
}

} // namespace spiration
