/**
 * @file window.h
 * @brief 窗口定义。
 * @author clk
 */

#pragma once

#include <spiration/ui/cursor.h>
#include <spiration/ui/widget.h>
#include <memory>
#include <string>
#include <functional>
#include <cstdint>

namespace spiration {

using void_function = std::function<void(void*)>;

/**
 * @brief 窗口创建参数。
 */
struct window_parameters {
    std::string title = ""; 
    int32_t width = 0;
    int32_t height = 0;
    bool resizable = true;
    bool visible = true;
    bool fullscreen = false;
    bool decorated = true;
};

/**
 * @brief 窗口类。
 */
class window {
public:
    window(const window&) = delete;
    window& operator=(const window&) = delete;
    
    window(window&& other) noexcept;
    window& operator=(window&& other) noexcept;
    
    static std::shared_ptr<window> create();
    static std::shared_ptr<window> create(const window_parameters& params);
    
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
    
    virtual void request_repaint() = 0;

    /**
     * @brief 请求在下一帧重新布局。
     */
    virtual void request_layout() {}

    /**
     * @brief 设置鼠标样式。
     * @param type 鼠标样式。
     */
    virtual void set_cursor(cursor_style type) { (void)type; }

    /**
     * @brief 设置鼠标捕获。
     * @param type 鼠标样式。
     */
    virtual void set_mouse_capture(bool capture) = 0;

    virtual void set_widget(std::unique_ptr<widget> widget) = 0;

    /**
     * @brief 获取当前平台渲染器，供子 widget 在布局阶段测量文本等使用。
     */
    virtual std::shared_ptr<class renderer> get_renderer() const = 0;
    
protected:
    window();
    
private:
    static std::shared_ptr<window> create_window(const window_parameters& params) {
        (void)params;
        return nullptr;
    };
    virtual bool initialize(const window_parameters& params) = 0;
    virtual void shutdown() = 0;
};

}
