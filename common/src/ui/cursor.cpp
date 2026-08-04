/**
 * @file cursor.cpp
 * @brief 指针管理器实现。
 * @author clk
 */

#include <ui/cursor.h>
#include <ui/widget.h>
#include <window/window.h>

namespace spiration {

cursor_manager& cursor_manager::instance() {
    static cursor_manager inst;
    return inst;
}

void cursor_manager::set_window(window* w) {
    window_ = w;
}

void cursor_manager::apply(cursor_type c) {
    if (c == current_) {
        return;
    }
    current_ = c;
    if (window_) {
        window_->set_cursor(c);
    }
}

void cursor_manager::update(widget* root, float x, float y) {
    cursor_type c = cursor_type::default_cursor;
    if (root) {
        if (widget* hovered = root->hit_test_hover(x, y)) {
            point p = hovered->to_screen(0.0f, 0.0f);
            c = hovered->effective_cursor(x - p.x, y - p.y);
        }
    }
    apply(c);
}

} // namespace spiration
