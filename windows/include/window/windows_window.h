/**
 * @file windows_window.h
 * @brief Windows 平台窗口实现。
 * @author clk
 */

#pragma once

#include <renderer/renderer.h>
#include <ui/widget.h>
#include <window/window.h>
#include <Windows.h>
#include <memory>
#include <functional>
#include <string>

namespace spiration {

/**
 * @brief Windows 平台窗口实现。
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

private:
    bool initialize(const window_params& params) override;
    void shutdown() override;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void UpdateDPIScale();
    void AdjustWindowForFullscreen();
    void RestoreWindowFromFullscreen();
    bool CreateRenderer();
    bool CreateWindowClass();
    bool CreateActualWindow(const window_params& params);
    void NotifyWidgetResize();

    HWND m_hWnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    std::wstring m_WindowClassName;

    bool m_ShouldClose = false;
    bool m_IsFullscreen = false;
    bool m_NeedsRepaint = true;

    RECT m_WindowRectBeforeFullscreen = {0};
    DWORD m_WindowStyleBeforeFullscreen = 0;
    DWORD m_WindowExStyleBeforeFullscreen = 0;

    void_function m_OnClose = nullptr;
    void_function m_OnResize = nullptr;
    void_function m_OnKey = nullptr;
    void_function m_OnMouse = nullptr;

    void* m_UserData = nullptr;
    uint32_t m_WindowId = 0;

    float m_DPIScale = 1.0f;

    static constexpr float DRAG_AREA_HEIGHT = 34.0f;

    static constexpr float RESIZE_BORDER_WIDTH = 6.0f;

    std::shared_ptr<renderer> m_Renderer = nullptr;
    std::unique_ptr<widget> m_Widget = nullptr;

    static uint32_t s_NextWindowId;
    friend class window;
};

} 