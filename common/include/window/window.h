/**
 * @file window.h
 * @brief 跨平台窗口抽象接口定义。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <memory>
#include <string>
#include <functional>
#include <cstdint>

namespace spiration {


using void_function = std::function<void(void*)>;

/**
 * @brief 窗口创建参数，用于配置窗口的初始状态。
 */
struct window_params {
    std::string title = "";
    int32_t width = 0;
    int32_t height = 0;
    bool resizable = true;
    bool visible = true;
    bool fullscreen = false;
    bool decorated = true;
    void* user_data = nullptr;
    
    void_function on_close = nullptr;
    void_function on_resize = nullptr;
    void_function on_key = nullptr;
    void_function on_mouse = nullptr;
};

/**
 * @brief 跨平台窗口抽象基类。
 */
class window {
public:
    window(const window&) = delete;
    window& operator=(const window&) = delete;
    
    window(window&& other) noexcept;
    window& operator=(window&& other) noexcept;
    
    static std::shared_ptr<window> create();
    static std::shared_ptr<window> create(const window_params& params);
    
    virtual ~window();
    
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void maximize() = 0;
    virtual void minimize() = 0;
    virtual void restore() = 0;
    virtual void close() = 0;
    
    virtual std::string title() const = 0;
    virtual void set_title(const std::string& title) = 0;
    
    virtual void get_size(int32_t& width, int32_t& height) const = 0;
    virtual void set_size(int32_t width, int32_t height) = 0;
    
    virtual void get_position(int32_t& x, int32_t& y) const = 0;
    virtual void set_position(int32_t x, int32_t y) = 0;
    
    virtual bool is_visible() const = 0;
    virtual bool is_maximized() const = 0;
    virtual bool is_minimized() const = 0;
    virtual bool is_fullscreen() const = 0;
    
    virtual void set_fullscreen(bool fullscreen) = 0;
    
    virtual void* native_handle() const = 0;
    
    virtual void loop() = 0;
    virtual bool should_close() const = 0;
    
    virtual void* user_data() const = 0;
    virtual void set_user_data(void* data) = 0;
    
    virtual void request_repaint() = 0;
    
    virtual void set_on_close(void_function callback) = 0;
    virtual void set_on_resize(void_function callback) = 0;
    virtual void set_on_key(void_function callback) = 0;
    virtual void set_on_mouse(void_function callback) = 0;

    virtual void set_mouse_capture(bool capture) = 0;

    virtual void set_widget(std::unique_ptr<widget> widget) = 0;
    
protected:
    window();
    
private:
    virtual bool initialize(const window_params& params) = 0;
    virtual void shutdown() = 0;
    
    static std::shared_ptr<window> create_window(const window_params& params) {
        return nullptr;
    };
};

}