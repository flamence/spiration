/**
 * @file macos_window.h
 * @brief macOS 平台窗口实现。
 * @author clk
 */

#pragma once

#include <renderer/renderer.h>
#include <ui/widget.h>
#include <window/window.h>
#include <memory>
#include <functional>
#include <string>

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#else
typedef struct objc_object NSWindow;
typedef struct objc_object NSView;
typedef struct objc_object NSApplication;
typedef struct objc_object CAMetalLayer;
#endif

namespace spiration {

/**
 * @brief macOS 平台窗口实现。
 */
class Window : public window {
public:
    Window() = default;
    ~Window() override;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

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
    void set_mouse_capture(bool capture) override;

    void set_widget(std::unique_ptr<widget> widget) override;
    widget* get_widget() const { return m_Widget.get(); }

    void_function on_mouse() const { return m_OnMouse; }
    void_function on_key() const { return m_OnKey; }
    float backing_scale() const { return m_BackingScale; }
    void set_backing_scale(float s) { m_BackingScale = s; }

private:
    bool initialize(const window_params& params) override;
    void shutdown() override;

    bool create_cocoa_window(const window_params& params);
    bool create_renderer();
    void notify_widget_resize();

    NSWindow* m_NSWindow = nullptr;
    NSView* m_NSView = nullptr;
    CAMetalLayer* m_MetalLayer = nullptr;

    std::string m_Title;
    int32_t m_Width = 800;
    int32_t m_Height = 600;
    int32_t m_X = 0;
    int32_t m_Y = 0;
    float m_BackingScale = 2.0f;

    bool m_ShouldClose = false;
    bool m_IsMaximized = false;
    bool m_IsMinimized = false;
    bool m_IsFullscreen = false;
    bool m_IsVisible = false;
    bool m_Initialized = false;

    double m_PrevFrameX = 0;
    double m_PrevFrameY = 0;
    double m_PrevFrameW = 0;
    double m_PrevFrameH = 0;

    void_function m_OnClose = nullptr;
    void_function m_OnResize = nullptr;
    void_function m_OnKey = nullptr;
    void_function m_OnMouse = nullptr;

    std::shared_ptr<renderer> m_Renderer;
    std::unique_ptr<widget> m_Widget;
    void* m_UserData = nullptr;
    uint32_t m_WindowId = 0;

    static uint32_t s_NextWindowId;

    friend class SpirationContentView;
    bool m_IsDragging = false;
    bool m_IsResizing = false;
    int m_ResizeEdge = 0;
    double m_DragStartX = 0, m_DragStartY = 0;
    double m_DragStartMouseX = 0, m_DragStartMouseY = 0;
    double m_DragStartW = 0, m_DragStartH = 0;
};

} // namespace spiration
