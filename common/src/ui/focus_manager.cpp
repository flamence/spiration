/**
 * @file focus_manager.cpp
 * @brief 全局焦点管理器实现。
 * @author clk
 */

#include <ui/focus_manager.h>
#include <ui/widget.h>

namespace spiration {

focus_manager& focus_manager::instance() {
    static focus_manager inst;
    return inst;
}

void focus_manager::request_focus(widget* w) {
    if (!w || !w->is_focusable()) return;
    if (w == focused_) return;
    if (focused_) {
        focused_->set_focused(false);
    }
    focused_ = w;
    w->set_focused(true);
}

void focus_manager::clear_focus() {
    if (!focused_) return;
    focused_->set_focused(false);
    focused_ = nullptr;
}

void focus_manager::on_widget_destroyed(widget* w) {
    if (focused_ == w) focused_ = nullptr;
}

} // namespace spiration
