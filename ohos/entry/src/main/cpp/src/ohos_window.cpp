/**
 * @file ohos_window.cpp
 * @brief OHOS 窗口实现。
 * @author clk
 */

#include "ohos_window.h"
#include "ohos_renderer.h"
#include <utils/console.h>

namespace spiration {

window::window() = default;
window::~window() = default;

ohos_window::ohos_window() = default;

ohos_window::~ohos_window() = default;

bool ohos_window::initialize(const window_params& params) {
    title_ = params.title;
    width_ = params.width > 0 ? params.width : 1200;
    height_ = params.height > 0 ? params.height : 800;
    visible_ = params.visible;
    user_data_ = params.user_data;
    on_close_ = params.on_close;
    on_resize_ = params.on_resize;
    on_key_ = params.on_key;
    on_mouse_ = params.on_mouse;
    return true;
}

void ohos_window::show() { visible_ = true; }
void ohos_window::hide() { visible_ = false; }

void ohos_window::close() {
    if (on_close_) on_close_(nullptr);
}

void ohos_window::set_title(const std::string& title) {
    title_ = title;
}

void ohos_window::get_size(int32_t& width, int32_t& height) const {
    width = width_;
    height = height_;
}

void ohos_window::set_size(int32_t width, int32_t height) {
    width_ = width;
    height_ = height;
    if (root_widget_) {
        root_widget_->width = static_cast<float>(width);
        root_widget_->height = static_cast<float>(height);
        root_widget_->layout();
    }
}

void* ohos_window::native_handle() const {
    return native_window_;
}

void ohos_window::tick(float dt_ms) {
    if (root_widget_) root_widget_->tick(dt_ms);
}

void ohos_window::render(const std::shared_ptr<renderer>& r) {
    if (!root_widget_ || !r) return;
    r->begin_frame();
    root_widget_->paint(r);
    r->end_frame();
}

void ohos_window::request_repaint() {
    auto r = renderer_.lock();
    if (r && root_widget_) {
        r->begin_frame();
        root_widget_->paint(r);
        r->end_frame();
    }
}

void ohos_window::set_widget(std::unique_ptr<widget> w) {
    root_widget_ = std::move(w);
    if (root_widget_) {
        root_widget_->set_repaint_callback([this]() {
            request_repaint();
        });
        root_widget_->set_window_action_callback([this](int action) {
            switch (action) {
                case widget::action_close:    close();               break;
                case widget::action_maximize:
                    if (on_maximize_) on_maximize_(nullptr);
                    break;
                case widget::action_minimize:
                    if (on_minimize_) on_minimize_(nullptr);
                    break;
                default: break;
            }
        });
        root_widget_->layout();
    }
}

void ohos_window::resize_widget(int32_t logical_w, int32_t logical_h) {
    if (!root_widget_) return;
    root_widget_->width = static_cast<float>(logical_w);
    root_widget_->height = static_cast<float>(logical_h);
    root_widget_->layout();
}

void ohos_window::on_touch_event(float x, float y, int action) {
    if (!root_widget_) return;

    mouse_event_data mouse;
    mouse.position.x = x;
    mouse.position.y = y;
    mouse.button = mouse_button::left;

    switch (action) {
        case 0: mouse.action = mouse_action::down; break; // ACTION_DOWN
        case 1: mouse.action = mouse_action::up; break;   // ACTION_UP
        case 2: mouse.action = mouse_action::move; break; // ACTION_MOVE
        default: return;
    }

    root_widget_->handle_event(event_type::mouse, &mouse);

    /* 在 appbar 背景区域按下左键时触发窗口拖拽 */
    if (action == 0 && on_start_move_ && !mouse.consumed && y < 34.0f) {
        on_start_move_(nullptr);
    }
}

} // namespace spiration
