/**
 * @file ohos_window.h
 * @brief OHOS 平台窗口。
 * @author clk
 */

#pragma once

#include <window/window.h>
#include <ui/widget.h>
#include <memory>
#include <string>
#include <cstdint>

namespace spiration {

/**
 * @brief OHOS 窗口实现。
 */
class ohos_window : public window {
public:
    ohos_window();
    ~ohos_window() override;

    void show() override;
    void hide() override;
    void maximize() override {}
    void minimize() override {}
    void restore() override {}
    void close() override;

    std::string title() const override { return title_; }
    void set_title(const std::string& title) override;

    void get_size(int32_t& width, int32_t& height) const override;
    void set_size(int32_t width, int32_t height) override;

    void get_position(int32_t& x, int32_t& y) const override { x = 0; y = 0; }
    void set_position(int32_t x, int32_t y) override {}

    bool is_visible() const override { return visible_; }
    bool is_maximized() const override { return false; }
    bool is_minimized() const override { return false; }
    bool is_fullscreen() const override { return false; }
    void set_fullscreen(bool fullscreen) override {}

    void* native_handle() const override;
    void loop() override {}
    bool should_close() const override { return false; }

    void* user_data() const override { return user_data_; }
    void set_user_data(void* data) override { user_data_ = data; }

    void request_repaint() override;

    void set_on_close(void_function callback) override { on_close_ = callback; }
    void set_on_resize(void_function callback) override { on_resize_ = callback; }
    void set_on_key(void_function callback) override { on_key_ = callback; }
    void set_on_mouse(void_function callback) override { on_mouse_ = callback; }
    void set_mouse_capture(bool capture) override {}
    void set_on_maximize(void_function callback) { on_maximize_ = callback; }
    void set_on_minimize(void_function callback) { on_minimize_ = callback; }
    void set_on_start_move(void_function callback) { on_start_move_ = callback; }

    void set_widget(std::unique_ptr<widget> w) override;

    void set_renderer(const std::shared_ptr<renderer>& r) { renderer_ = r; }

    void on_touch_event(float x, float y, int action);

    void resize_widget(int32_t logical_w, int32_t logical_h);

    void tick(float dt_ms);

    void render(const std::shared_ptr<renderer>& r);

    bool initialize(const window_params& params) override;

protected:
    void shutdown() override {}

private:
    std::string title_;
    int32_t width_ = 1200;
    int32_t height_ = 800;
    std::unique_ptr<widget> root_widget_;
    std::weak_ptr<renderer> renderer_;
    void* native_window_ = nullptr;
    void* user_data_ = nullptr;
    bool visible_ = false;

    void_function on_close_ = nullptr;
    void_function on_resize_ = nullptr;
    void_function on_key_ = nullptr;
    void_function on_mouse_ = nullptr;
    void_function on_maximize_ = nullptr;
    void_function on_minimize_ = nullptr;
    void_function on_start_move_ = nullptr;
};

} // namespace spiration
