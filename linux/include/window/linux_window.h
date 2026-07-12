/**
 * @file linux_window.h
 * @brief Linux 平台窗口实现（X11 / xcb）。
 * @author clk
 */

#pragma once

#include <renderer/renderer.h>
#include <ui/widget.h>
#include <window/window.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>

namespace spiration {

/**
 * @brief Linux 平台窗口实现。
 *
 * 基于 X11 (Xlib) 实现窗口框架，支持：
 * - GLX OpenGL 上下文
 * - DPI 感知
 * - 全屏切换
 * - OpenGL 渲染器绑定
 * - Widget 树绑定与消息分发
 */
class linux_window : public window {
public:
    linux_window() = default;
    ~linux_window() override;

    linux_window(const linux_window&) = delete;
    linux_window& operator=(const linux_window&) = delete;

    linux_window(linux_window&& other) noexcept;
    linux_window& operator=(linux_window&& other) noexcept;

    void show() override;
    void hide() override;
    void maximize() override;
    void minimize() override;
    void restore() override;
    void close() override;

    std::string title() const override;
    void set_title(const std::string& title) override;

    void get_size(int32_t& width, int32_t& height) const override;
    void set_size(int32_t width, int32_t height) override;

    void get_position(int32_t& x, int32_t& y) const override;
    void set_position(int32_t x, int32_t y) override;

    bool is_visible() const override;
    bool is_maximized() const override;
    bool is_minimized() const override;
    bool is_fullscreen() const override;
    void set_fullscreen(bool fullscreen) override;

    void* native_handle() const override;

    void loop() override;
    bool should_close() const override;

    void* user_data() const override;
    void set_user_data(void* data) override;

    void request_repaint() override;

    void set_on_close(void_function callback) override;
    void set_on_resize(void_function callback) override;
    void set_on_key(void_function callback) override;
    void set_on_mouse(void_function callback) override;

    void set_widget(std::unique_ptr<widget> widget) override;

    bool initialize(const window_params& params) override;
    void shutdown() override;

private:
    bool connect_to_x11();
    bool select_fb_config();
    bool create_x11_window(const window_params& params);
    bool create_gl_context();
    bool create_renderer();
    void process_events();
    void handle_expose();
    void handle_resize(uint32_t width, uint32_t height);
    void handle_key_press(XKeyEvent* event);
    void handle_key_release(XKeyEvent* event);
    void handle_button_press(XButtonEvent* event);
    void handle_button_release(XButtonEvent* event);
    void handle_mouse_motion(XMotionEvent* event);
    void handle_mouse_wheel(XButtonEvent* event, bool up);
    void update_dpi();
    void notify_widget_resize();
    int hit_test_edge(float x, float y) const;

    Display* display_ = nullptr;
    ::linux_window window_ = 0;
    Atom delete_atom_ = 0;
    Atom wm_state_atom_ = 0;
    Atom wm_state_fullscreen_atom_ = 0;
    Atom wm_state_max_v_atom_ = 0;
    Atom wm_state_max_h_atom_ = 0;
    Atom net_wm_name_atom_ = 0;
    Atom net_wm_icon_atom_ = 0;
    Atom utf8_string_atom_ = 0;

    int screen_number_ = 0;

    GLXContext gl_context_ = nullptr;
    GLXFBConfig glx_fb_config_ = nullptr;
    XVisualInfo* glx_visual_ = nullptr;

    std::string title_;
    int32_t width_ = 800;
    int32_t height_ = 600;
    int32_t x_ = 0;
    int32_t y_ = 0;
    float dpi_scale_ = 1.0f;

    bool should_close_ = false;
    bool is_maximized_ = false;
    bool is_minimized_ = false;
    bool is_fullscreen_ = false;
    bool is_visible_ = false;
    bool initialized_ = false;

    int32_t prev_width_ = 0;
    int32_t prev_height_ = 0;
    int32_t prev_x_ = 0;
    int32_t prev_y_ = 0;

    void_function on_close_ = nullptr;
    void_function on_resize_ = nullptr;
    void_function on_key_ = nullptr;
    void_function on_mouse_ = nullptr;

    std::shared_ptr<renderer> renderer_;
    std::unique_ptr<widget> widget_;
    void* user_data_ = nullptr;
    uint32_t window_id_ = 0;

    std::unordered_map<KeySym, bool> key_state_;

    int32_t last_mouse_x_ = 0;
    int32_t last_mouse_y_ = 0;

    bool is_dragging_ = false;
    int drag_offset_x_ = 0;
    int drag_offset_y_ = 0;
    Time last_click_time_ = 0;
    float last_click_x_ = 0.0f;
    float last_click_y_ = 0.0f;

    bool is_resizing_ = false;
    int resize_flags_ = 0;
    int resize_start_w_ = 0;
    int resize_start_h_ = 0;
    int resize_start_x_ = 0;
    int resize_start_y_ = 0;
    int resize_start_mx_ = 0;
    int resize_start_my_ = 0;

    static constexpr float RESIZE_MARGIN = 6.0f;

    static constexpr float DRAG_AREA_HEIGHT = 34.0f;

    static uint32_t next_window_id_;
};

} // namespace spiration
