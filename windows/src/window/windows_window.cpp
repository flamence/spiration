/**
 * @file windows_window.cpp
 * @brief Windows 平台窗口实现（Win32 API）。
 * @author clk
 */

#include <resource.h>
#include <ui/point.h>
#include <ui/size.h>
#include <utils/console.h>
#include <window/windows_window.h>
#include <memory>
#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <imm.h>
#include <iostream>
#include <cmath>
#include <functional>


namespace spiration {

uint32_t Window::s_NextWindowId = 1;


Window::~Window() {
    shutdown();
}

Window::Window(Window&& other) noexcept
    : m_hWnd(other.m_hWnd)
    , m_hInstance(other.m_hInstance)
    , m_WindowClassName(std::move(other.m_WindowClassName))
    , m_ShouldClose(other.m_ShouldClose)
    , m_IsFullscreen(other.m_IsFullscreen)
    , m_WindowRectBeforeFullscreen(other.m_WindowRectBeforeFullscreen)
    , m_WindowStyleBeforeFullscreen(other.m_WindowStyleBeforeFullscreen)
    , m_WindowExStyleBeforeFullscreen(other.m_WindowExStyleBeforeFullscreen)
    , m_OnClose(std::move(other.m_OnClose))
    , m_OnResize(std::move(other.m_OnResize))
    , m_OnKey(std::move(other.m_OnKey))
    , m_OnMouse(std::move(other.m_OnMouse))
    , m_UserData(other.m_UserData)
    , m_WindowId(other.m_WindowId)
    , m_DPIScale(other.m_DPIScale)
    , m_Renderer(std::move(other.m_Renderer))
    , m_Widget(std::move(other.m_Widget)) {

    other.m_hWnd = nullptr;
    other.m_hInstance = nullptr;
    other.m_ShouldClose = false;
    other.m_IsFullscreen = false;
    other.m_UserData = nullptr;
    other.m_WindowId = 0;
    other.m_DPIScale = 1.0f;

    if (m_hWnd) {
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        shutdown();

        m_hWnd = other.m_hWnd;
        m_hInstance = other.m_hInstance;
        m_WindowClassName = std::move(other.m_WindowClassName);
        m_ShouldClose = other.m_ShouldClose;
        m_IsFullscreen = other.m_IsFullscreen;
        m_WindowRectBeforeFullscreen = other.m_WindowRectBeforeFullscreen;
        m_WindowStyleBeforeFullscreen = other.m_WindowStyleBeforeFullscreen;
        m_WindowExStyleBeforeFullscreen = other.m_WindowExStyleBeforeFullscreen;
        m_OnClose = std::move(other.m_OnClose);
        m_OnResize = std::move(other.m_OnResize);
        m_OnKey = std::move(other.m_OnKey);
        m_OnMouse = std::move(other.m_OnMouse);
        m_UserData = other.m_UserData;
        m_WindowId = other.m_WindowId;
        m_DPIScale = other.m_DPIScale;
        m_Renderer = std::move(other.m_Renderer);
        m_Widget = std::move(other.m_Widget);

        other.m_hWnd = nullptr;
        other.m_hInstance = nullptr;
        other.m_ShouldClose = false;
        other.m_IsFullscreen = false;
        other.m_UserData = nullptr;
        other.m_WindowId = 0;
        other.m_DPIScale = 1.0f;

        if (m_hWnd) {
            SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
    }
    return *this;
}


LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Window* pThis = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!pThis && uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        if (pThis) {
            pThis->m_hWnd = hwnd;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        }
    }
    return pThis ? pThis->HandleMessage(uMsg, wParam, lParam) : DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_NCHITTEST: {
        
        if (!m_hWnd) break;
        POINT pt = { static_cast<SHORT>(LOWORD(lParam)),
                     static_cast<SHORT>(HIWORD(lParam)) };
        ScreenToClient(m_hWnd, &pt);
        float xDIP = static_cast<float>(pt.x) / m_DPIScale;
        float yDIP = static_cast<float>(pt.y) / m_DPIScale;
        int32_t w, h;
        get_size(w, h);

        bool onTop    = yDIP < RESIZE_BORDER_WIDTH;
        bool onBottom = yDIP > (static_cast<float>(h) - RESIZE_BORDER_WIDTH);
        bool onLeft   = xDIP < RESIZE_BORDER_WIDTH;
        bool onRight  = xDIP > (static_cast<float>(w) - RESIZE_BORDER_WIDTH);

        if (onTop    && onLeft)  return HTTOPLEFT;
        if (onTop    && onRight) return HTTOPRIGHT;
        if (onBottom && onLeft)  return HTBOTTOMLEFT;
        if (onBottom && onRight) return HTBOTTOMRIGHT;
        if (onTop)    return HTTOP;
        if (onBottom) return HTBOTTOM;
        if (onLeft)   return HTLEFT;
        if (onRight)  return HTRIGHT;

        return HTCLIENT;
    }

    case WM_NCCALCSIZE: {
        return 0;
    }

    case WM_NCPAINT:
        return TRUE;

    case WM_NCACTIVATE:
        return TRUE;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(m_hWnd, &ps);
        if (m_Widget && m_Renderer) {
            m_Widget->layout();
            m_Renderer->begin_frame();
            m_Widget->paint(m_Renderer);
            m_Renderer->end_frame();
        }
        EndPaint(m_hWnd, &ps);
        return TRUE;
    }

    case WM_DPICHANGED: {
        UINT dpi = HIWORD(wParam);
        m_DPIScale = static_cast<float>(dpi) / 96.0f;

        RECT* prcNew = reinterpret_cast<RECT*>(lParam);
        if (prcNew) {
            SetWindowPos(m_hWnd, nullptr,
                         prcNew->left, prcNew->top,
                         prcNew->right - prcNew->left,
                         prcNew->bottom - prcNew->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }

        if (m_Renderer) {
            int32_t widthDIP, heightDIP;
            get_size(widthDIP, heightDIP);
            m_Renderer->resize(static_cast<uint32_t>(widthDIP),
                               static_cast<uint32_t>(heightDIP));
        }

        NotifyWidgetResize();
        if (m_OnResize) m_OnResize(this);

        InvalidateRect(m_hWnd, nullptr, FALSE);
        return TRUE;
    }

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        if (mmi) {
            float minW = 400.0f, minH = 300.0f;
            mmi->ptMinTrackSize.x = static_cast<LONG>(std::lround(minW * m_DPIScale));
            mmi->ptMinTrackSize.y = static_cast<LONG>(std::lround(minH * m_DPIScale));
            
            RECT workArea;
            if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
                mmi->ptMaxPosition.x = workArea.left;
                mmi->ptMaxPosition.y = workArea.top;
                mmi->ptMaxSize.x = workArea.right - workArea.left;
                mmi->ptMaxSize.y = workArea.bottom - workArea.top;
            }
        }
        return TRUE;
    }

    case WM_DESTROY:
        m_ShouldClose = true;
        if (m_OnClose) m_OnClose(this);
        m_hWnd = nullptr;
        return TRUE;

    case WM_CLOSE:
        m_ShouldClose = true;
        if (m_OnClose) m_OnClose(this);
        return TRUE;

    case WM_SIZE: {
        int widthPx = LOWORD(lParam);
        int heightPx = HIWORD(lParam);
        if (widthPx > 0 && heightPx > 0 && m_Renderer) {
            uint32_t widthDIP = static_cast<uint32_t>(std::lround(widthPx / m_DPIScale));
            uint32_t heightDIP = static_cast<uint32_t>(std::lround(heightPx / m_DPIScale));
            m_Renderer->resize(widthDIP, heightDIP);
        }
        NotifyWidgetResize();
        if (m_OnResize) m_OnResize(this);
        InvalidateRect(m_hWnd, nullptr, FALSE);
        return TRUE;
    }

    case WM_SYSKEYDOWN:
        if (wParam == VK_F4) {
            return DefWindowProcW(m_hWnd, uMsg, wParam, lParam);
        }
        if (wParam == VK_SPACE) {
            return DefWindowProcW(m_hWnd, uMsg, wParam, lParam);
        }
        if (m_OnKey) m_OnKey(this);
        break;

    case WM_KEYDOWN:
        if (wParam == VK_F11) {
            set_fullscreen(!m_IsFullscreen);
            return TRUE;
        }
        if (wParam == VK_ESCAPE && m_IsFullscreen) {
            set_fullscreen(false);
            return TRUE;
        }
        if (m_Widget) {
            key_event_data ked;
            ked.key_code = static_cast<int>(wParam);
            ked.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            ked.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            ked.alt   = (GetKeyState(VK_MENU) & 0x8000) != 0;
            m_Widget->handle_event(event_type::keyboard, &ked);
        }
        break;

    case WM_CHAR:
        if (m_Widget) {
            key_event_data ked;
            ked.codepoint = static_cast<unsigned int>(wParam);
            m_Widget->handle_event(event_type::keyboard, &ked);
            if (ked.ime_x >= 0) {
                HIMC hIMC = ImmGetContext(m_hWnd);
                if (hIMC) {
                    COMPOSITIONFORM cf = { CFS_POINT, { (LONG)ked.ime_x, (LONG)ked.ime_y }, {} };
                    ImmSetCompositionWindow(hIMC, &cf);
                    ImmReleaseContext(m_hWnd, hIMC);
                }
            }
        }
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (m_OnKey) m_OnKey(this);
        break;

    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP: {
        if (m_OnMouse) m_OnMouse(this);

        POINT clientPos;
        if (uMsg == WM_MOUSEWHEEL) {
            
            clientPos.x = GET_X_LPARAM(lParam);
            clientPos.y = GET_Y_LPARAM(lParam);
            ScreenToClient(m_hWnd, &clientPos);
        } else {
            clientPos.x = GET_X_LPARAM(lParam);
            clientPos.y = GET_Y_LPARAM(lParam);
        }

        float xDIP = static_cast<float>(clientPos.x) / m_DPIScale;
        float yDIP = static_cast<float>(clientPos.y) / m_DPIScale;

        console::debug("mouse msg=0x%04X raw=(%d,%d) dip=(%.2f,%.2f) scale=%.2f",
                       uMsg, clientPos.x, clientPos.y, xDIP, yDIP, m_DPIScale);

        mouse_button btn = mouse_button::none;
        switch (uMsg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP:   btn = mouse_button::left; break;
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:   btn = mouse_button::right; break;
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:   btn = mouse_button::middle; break;
            default: break;
        }

        mouse_action act = mouse_action::move;
        switch (uMsg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
                act = mouse_action::down; break;
            case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
                act = mouse_action::up; break;
            case WM_MOUSEWHEEL:
                act = mouse_action::wheel; break;
            default:
                act = mouse_action::move; break;
        }

        mouse_event_data data;
        data.position = { xDIP, yDIP };
        data.button = btn;
        data.action = act;
        data.wheel_delta = (uMsg == WM_MOUSEWHEEL) ? GET_WHEEL_DELTA_WPARAM(wParam) : 0;

        if (m_Widget) {
            m_Widget->handle_event(event_type::mouse, &data);
        }

        if (!data.consumed && yDIP < DRAG_AREA_HEIGHT) {
            if (uMsg == WM_LBUTTONDOWN) {
                POINT screenPt = { clientPos.x, clientPos.y };
                ClientToScreen(m_hWnd, &screenPt);
                ReleaseCapture();
                SendMessageW(m_hWnd, WM_NCLBUTTONDOWN, HTCAPTION,
                             MAKELPARAM(screenPt.x, screenPt.y));
            } else if (uMsg == WM_LBUTTONDBLCLK) {
                if (is_maximized()) {
                    restore();
                } else {
                    maximize();
                }
            }
        }

        return TRUE;
    }
    }
    return DefWindowProc(m_hWnd, uMsg, wParam, lParam);
}

void Window::set_mouse_capture(bool capture) {
    if (capture) {
        SetCapture(m_hWnd);
    } else {
        ReleaseCapture();
    }
}

bool Window::initialize(const window_params& params) {
    m_hInstance = GetModuleHandle(nullptr);
    if (!m_hInstance) return false;

    m_WindowId = s_NextWindowId++;
    if (!CreateWindowClass()) return false;
    if (!CreateActualWindow(params)) return false;

    UpdateDPIScale();

    if (!CreateRenderer()) return false;

    m_OnClose = params.on_close;
    m_OnResize = params.on_resize;
    m_OnKey = params.on_key;
    m_OnMouse = params.on_mouse;
    m_UserData = params.user_data;

    return true;
}

void Window::shutdown() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    if (!m_WindowClassName.empty() && m_hInstance) {
        UnregisterClassW(m_WindowClassName.c_str(), m_hInstance);
        m_WindowClassName.clear();
    }
    if (m_Renderer) {
        m_Renderer->shutdown();
        m_Renderer.reset();
    }
}

bool Window::CreateWindowClass() {
    m_WindowClassName = L"SpirationWindowClass_" + std::to_wstring(m_WindowId);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.hbrBackground = nullptr;  
    wc.lpszClassName = m_WindowClassName.c_str();
    wc.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON_SMALL));
    return RegisterClassExW(&wc) != 0;
}

bool Window::CreateActualWindow(const window_params& params) {
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;
    if (!params.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    if (!params.decorated) {
        
        
        style = WS_POPUP | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
        exStyle = WS_EX_APPWINDOW;
    }

    
    int widthPx = static_cast<int>(std::lround(params.width * m_DPIScale));
    int heightPx = static_cast<int>(std::lround(params.height * m_DPIScale));

    int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - widthPx) / 2;
    int y = (screenHeight - heightPx) / 2;

    std::wstring wideTitle(params.title.begin(), params.title.end());
    m_hWnd = CreateWindowExW(
        exStyle, m_WindowClassName.c_str(), wideTitle.c_str(),
        style, x, y, widthPx, heightPx,
        nullptr, nullptr, m_hInstance, this
    );
    if (!m_hWnd) return false;

    
    SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                          &corner, sizeof(corner));

    if (params.fullscreen) set_fullscreen(true);
    return true;
}

bool Window::CreateRenderer() {
    if (m_Renderer) return true;
    m_Renderer = renderer::create_direct2d_renderer();
    if (!m_Renderer) return false;
    if (!m_Renderer->initialize(m_hWnd)) {
        m_Renderer.reset();
        return false;
    }
    
    int32_t w, h;
    get_size(w, h);
    m_Renderer->resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    return true;
}

void Window::UpdateDPIScale() {
    if (!m_hWnd) {
        m_DPIScale = 1.0f;
        return;
    }
    HDC hdc = GetDC(m_hWnd);
    UINT dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(m_hWnd, hdc);
    m_DPIScale = static_cast<float>(dpi) / 96.0f;
}

void Window::NotifyWidgetResize() {
    if (!m_Widget) return;
    int32_t widthDIP, heightDIP;
    get_size(widthDIP, heightDIP);
    size sizeData = { static_cast<float>(widthDIP), static_cast<float>(heightDIP) };
    m_Widget->handle_event(event_type::window_resize, &sizeData);
}


void Window::AdjustWindowForFullscreen() {
    if (m_IsFullscreen || !m_hWnd) return;
    GetWindowRect(m_hWnd, &m_WindowRectBeforeFullscreen);
    m_WindowStyleBeforeFullscreen = GetWindowLong(m_hWnd, GWL_STYLE);
    m_WindowExStyleBeforeFullscreen = GetWindowLong(m_hWnd, GWL_EXSTYLE);

    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
        SetWindowLong(m_hWnd, GWL_STYLE, WS_POPUP);
        SetWindowLong(m_hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
        SetWindowPos(m_hWnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        m_IsFullscreen = true;

        
        int32_t w, h;
        get_size(w, h);
        if (m_Renderer) m_Renderer->resize(w, h);
        NotifyWidgetResize();
    }
}

void Window::RestoreWindowFromFullscreen() {
    if (!m_IsFullscreen) return;
    SetWindowLong(m_hWnd, GWL_STYLE, m_WindowStyleBeforeFullscreen);
    SetWindowLong(m_hWnd, GWL_EXSTYLE, m_WindowExStyleBeforeFullscreen);
    SetWindowPos(m_hWnd, nullptr,
                 m_WindowRectBeforeFullscreen.left, m_WindowRectBeforeFullscreen.top,
                 m_WindowRectBeforeFullscreen.right - m_WindowRectBeforeFullscreen.left,
                 m_WindowRectBeforeFullscreen.bottom - m_WindowRectBeforeFullscreen.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    m_IsFullscreen = false;

    int32_t w, h;
    get_size(w, h);
    if (m_Renderer) m_Renderer->resize(w, h);
    NotifyWidgetResize();
}

void Window::set_fullscreen(bool fullscreen) {
    if (fullscreen == m_IsFullscreen) return;
    if (fullscreen) AdjustWindowForFullscreen();
    else RestoreWindowFromFullscreen();
}


void Window::show() {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_SHOW);

        if (m_Widget && m_Renderer) {
            m_Widget->layout();
            m_Renderer->begin_frame();
            m_Widget->paint(m_Renderer);
            m_Renderer->end_frame();
        }

        m_NeedsRepaint = true;
    }
}
void Window::hide()   { if (m_hWnd) ShowWindow(m_hWnd, SW_HIDE); }
void Window::maximize(){ if (m_hWnd) ShowWindow(m_hWnd, SW_MAXIMIZE); }
void Window::minimize(){ if (m_hWnd) ShowWindow(m_hWnd, SW_MINIMIZE); }
void Window::restore() { if (m_hWnd) ShowWindow(m_hWnd, SW_RESTORE); }
void Window::close()   { if (m_hWnd) SendMessage(m_hWnd, WM_CLOSE, 0, 0); }

std::string Window::title() const {
    if (!m_hWnd) return "";
    wchar_t buffer[256] = {};
    GetWindowTextW(m_hWnd, buffer, 256);
    int len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
    std::string result(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, &result[0], len, nullptr, nullptr);
    result.pop_back(); 
    return result;
}

void Window::set_title(const std::string& title) {
    if (m_hWnd) SetWindowTextW(m_hWnd, std::wstring(title.begin(), title.end()).c_str());
}

void Window::get_size(int32_t& width, int32_t& height) const {
    if (!m_hWnd) {
        width = height = 0;
        return;
    }
    RECT rect = {};
    GetClientRect(m_hWnd, &rect);
    
    width  = static_cast<int32_t>(std::lround((rect.right - rect.left) / m_DPIScale));
    height = static_cast<int32_t>(std::lround((rect.bottom - rect.top) / m_DPIScale));
}

void Window::set_size(int32_t widthDIP, int32_t heightDIP) {
    if (!m_hWnd) return;
    
    int widthPx  = static_cast<int>(std::lround(widthDIP * m_DPIScale));
    int heightPx = static_cast<int>(std::lround(heightDIP * m_DPIScale));
    SetWindowPos(m_hWnd, nullptr, 0, 0, widthPx, heightPx,
                 SWP_NOMOVE | SWP_NOZORDER);
}

void Window::get_position(int32_t& x, int32_t& y) const {
    if (!m_hWnd) { x = y = 0; return; }
    RECT rect = {};
    GetWindowRect(m_hWnd, &rect);
    x = rect.left;
    y = rect.top;
}

void Window::set_position(int32_t x, int32_t y) {
    if (m_hWnd) SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

bool Window::is_visible() const    { return m_hWnd && IsWindowVisible(m_hWnd); }
bool Window::is_maximized() const  { return m_hWnd && IsZoomed(m_hWnd); }
bool Window::is_minimized() const  { return m_hWnd && IsIconic(m_hWnd); }
bool Window::is_fullscreen() const { return m_IsFullscreen; }

void* Window::native_handle() const { return m_hWnd; }


void Window::loop() {
    MSG msg;
    while (!m_ShouldClose) {
        bool processedInput = false;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            
            if (msg.message != WM_PAINT) {
                processedInput = true;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) m_ShouldClose = true;
        }

        if (m_Widget) {
            m_Widget->layout();
        }

        
        if (m_NeedsRepaint || processedInput) {
            if (m_hWnd) {
                InvalidateRect(m_hWnd, nullptr, FALSE);
                m_NeedsRepaint = false;
            }
        }

        
        if (!processedInput && !m_NeedsRepaint) {
            WaitMessage();
        }

        if (m_Widget) {
            static DWORD lastTick = GetTickCount();
            DWORD now = GetTickCount();
            float dt_ms = static_cast<float>(now - lastTick);
            lastTick = now;
            m_Widget->tick(dt_ms);
        }
    }
}

void Window::request_repaint() {
    if (m_hWnd) {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

bool Window::should_close() const { return m_ShouldClose; }

void* Window::user_data() const   { return m_UserData; }
void Window::set_user_data(void* data) { m_UserData = data; }

void Window::set_on_close(void_function callback)   { m_OnClose = callback; }
void Window::set_on_resize(void_function callback)  { m_OnResize = callback; }
void Window::set_on_key(void_function callback)     { m_OnKey = callback; }
void Window::set_on_mouse(void_function callback)   { m_OnMouse = callback; }

void Window::set_widget(std::unique_ptr<widget> widget) {
    m_Widget = std::move(widget);
    if (m_Widget) {
        m_Widget->set_repaint_callback([this]() { request_repaint(); });
        m_Widget->set_window_action_callback([this](int action) {
            switch (action) {
                case widget::action_minimize: minimize(); break;
                case widget::action_maximize:
                    if (is_maximized()) restore(); else maximize();
                    break;
                case widget::action_restore:  restore();  break;
                case widget::action_close:   close();     break;
            }
        });
    }
    NotifyWidgetResize();
    request_repaint();
}


std::shared_ptr<window> window::create() {
    return create(window_params{});
}

std::shared_ptr<window> window::create(const window_params& params) {
    auto win = std::make_shared<Window>();
    if (!win->initialize(params)) return nullptr;
    return win;
}

window::window() = default;
window::~window() = default;
window::window(window&&) noexcept = default;
window& window::operator=(window&&) noexcept = default;

} 