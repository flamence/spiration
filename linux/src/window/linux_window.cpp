/**
 * @file linux_window.cpp
 * @brief Linux 平台窗口实现。
 * @author clk
 */

#include <window/linux_window.h>
#include <ui/point.h>
#include <ui/size.h>
#include <utils/console.h>
#include <utils/platform.h>
#include <stb_image.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <X11/cursorfont.h>

namespace spiration {

uint32_t linux_window::next_window_id_ = 1;

static int x11_error_handler(Display* display, XErrorEvent* event) {
    char buf[256];
    XGetErrorText(display, event->error_code, buf, sizeof(buf));
    spiration::console::warning("X11 error: %s (request: %d, minor: %d)",
                     buf, event->request_code, event->minor_code);
    return 0;
}

linux_window::~linux_window() {
    shutdown();
}

linux_window::linux_window(linux_window&& other) noexcept
    : display_(other.display_)
    , window_(other.window_)
    , delete_atom_(other.delete_atom_)
    , wm_state_atom_(other.wm_state_atom_)
    , wm_state_fullscreen_atom_(other.wm_state_fullscreen_atom_)
    , wm_state_max_v_atom_(other.wm_state_max_v_atom_)
    , wm_state_max_h_atom_(other.wm_state_max_h_atom_)
    , net_wm_name_atom_(other.net_wm_name_atom_)
    , net_wm_icon_atom_(other.net_wm_icon_atom_)
    , utf8_string_atom_(other.utf8_string_atom_)
    , screen_number_(other.screen_number_)
    , gl_context_(other.gl_context_)
    , glx_visual_(other.glx_visual_)
    , title_(std::move(other.title_))
    , width_(other.width_)
    , height_(other.height_)
    , x_(other.x_)
    , y_(other.y_)
    , dpi_scale_(other.dpi_scale_)
    , should_close_(other.should_close_)
    , is_maximized_(other.is_maximized_)
    , is_minimized_(other.is_minimized_)
    , is_fullscreen_(other.is_fullscreen_)
    , is_visible_(other.is_visible_)
    , initialized_(other.initialized_)
    , on_close_(std::move(other.on_close_))
    , on_resize_(std::move(other.on_resize_))
    , on_key_(std::move(other.on_key_))
    , on_mouse_(std::move(other.on_mouse_))
    , renderer_(std::move(other.renderer_))
    , widget_(std::move(other.widget_))
    , user_data_(other.user_data_)
    , window_id_(other.window_id_)
{
    other.display_ = nullptr;
    other.window_ = 0;
    other.gl_context_ = nullptr;
    other.glx_visual_ = nullptr;
    other.is_dragging_ = false;
    other.net_wm_name_atom_ = 0;
    other.net_wm_icon_atom_ = 0;
    other.utf8_string_atom_ = 0;
    other.should_close_ = false;
    other.is_visible_ = false;
    other.initialized_ = false;
    other.user_data_ = nullptr;
    other.window_id_ = 0;
    other.dpi_scale_ = 1.0f;
}

linux_window& linux_window::operator=(linux_window&& other) noexcept {
    if (this != &other) {
        shutdown();

        display_ = other.display_;
                window_ = other.window_;
        delete_atom_ = other.delete_atom_;
        wm_state_atom_ = other.wm_state_atom_;
        wm_state_fullscreen_atom_ = other.wm_state_fullscreen_atom_;
        wm_state_max_v_atom_ = other.wm_state_max_v_atom_;
        wm_state_max_h_atom_ = other.wm_state_max_h_atom_;
        net_wm_name_atom_ = other.net_wm_name_atom_;
        net_wm_icon_atom_ = other.net_wm_icon_atom_;
        utf8_string_atom_ = other.utf8_string_atom_;
        screen_number_ = other.screen_number_;
        gl_context_ = other.gl_context_;
        glx_visual_ = other.glx_visual_;
        display_ = other.display_;
        title_ = std::move(other.title_);
        width_ = other.width_;
        height_ = other.height_;
        x_ = other.x_;
        y_ = other.y_;
        dpi_scale_ = other.dpi_scale_;
        should_close_ = other.should_close_;
        is_maximized_ = other.is_maximized_;
        is_minimized_ = other.is_minimized_;
        is_fullscreen_ = other.is_fullscreen_;
        is_visible_ = other.is_visible_;
        initialized_ = other.initialized_;
        on_close_ = std::move(other.on_close_);
        on_resize_ = std::move(other.on_resize_);
        on_key_ = std::move(other.on_key_);
        on_mouse_ = std::move(other.on_mouse_);
        renderer_ = std::move(other.renderer_);
        widget_ = std::move(other.widget_);
        user_data_ = other.user_data_;
        window_id_ = other.window_id_;

        other.display_ = nullptr;
        other.window_ = 0;
        other.gl_context_ = nullptr;
        other.glx_visual_ = nullptr;
        other.is_dragging_ = false;
        other.net_wm_name_atom_ = 0;
        other.net_wm_icon_atom_ = 0;
        other.utf8_string_atom_ = 0;
        other.should_close_ = false;
        other.is_visible_ = false;
        other.initialized_ = false;
        other.user_data_ = nullptr;
        other.window_id_ = 0;
        other.dpi_scale_ = 1.0f;
    }
    return *this;
}


void linux_window::show() {
    if (!display_ || !window_) return;
    XMapWindow(display_, window_);
    XFlush(display_);
    is_visible_ = true;
}

void linux_window::hide() {
    if (!display_ || !window_) return;
    XUnmapWindow(display_, window_);
    XFlush(display_);
    is_visible_ = false;
}

void linux_window::maximize() {
    if (!display_ || !window_) return;
    if (is_maximized_) return;

    prev_width_ = width_;
    prev_height_ = height_;
    prev_x_ = x_;
    prev_y_ = y_;

    XEvent xev = {};
    xev.type = ClientMessage;
    xev.xclient.window = window_;
    xev.xclient.message_type = wm_state_atom_;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = wm_state_max_h_atom_;
    xev.xclient.data.l[2] = wm_state_max_v_atom_;
    XSendEvent(display_, DefaultRootWindow(display_), False,
               SubstructureNotifyMask, &xev);
    XFlush(display_);

    is_maximized_ = true;
}

void linux_window::minimize() {
    if (!display_ || !window_) return;
    is_minimized_ = true;
    XIconifyWindow(display_, window_, screen_number_);
    XFlush(display_);
}

void linux_window::restore() {
    if (!display_ || !window_) return;
    is_minimized_ = false;

    if (is_fullscreen_) {
        set_fullscreen(false);
        return;
    }

    if (is_maximized_) {
        is_maximized_ = false;
        // 使用 EWMH 取消最大化
        XEvent xev = {};
        xev.type = ClientMessage;
        xev.xclient.window = window_;
        xev.xclient.message_type = wm_state_atom_;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
        xev.xclient.data.l[1] = wm_state_max_h_atom_;
        xev.xclient.data.l[2] = wm_state_max_v_atom_;
        XSendEvent(display_, DefaultRootWindow(display_), False,
                   SubstructureNotifyMask, &xev);
        XFlush(display_);
        return;
    }

    if (prev_width_ > 0 && prev_height_ > 0) {
        width_ = prev_width_;
        height_ = prev_height_;
    }
    auto* display = display_;
    XMoveResizeWindow(display, window_,
                      static_cast<int>(x_ * dpi_scale_),
                      static_cast<int>(y_ * dpi_scale_),
                      static_cast<unsigned int>(width_ * dpi_scale_),
                      static_cast<unsigned int>(height_ * dpi_scale_));
    XMapWindow(display, window_);
    XFlush(display);
    is_visible_ = true;
}

void linux_window::close() {
    should_close_ = true;
    if (on_close_) on_close_(this);
}

std::string linux_window::title() const {
    return title_;
}

void linux_window::set_title(const std::string& title) {
    title_ = title;
    if (display_ && window_) {
        auto* display = display_;
        XStoreName(display, window_, title.c_str());
        XChangeProperty(display, window_, net_wm_name_atom_, utf8_string_atom_,
                        8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(title.c_str()),
                        static_cast<int>(title.size()));
        XFlush(display);
    }
}

void linux_window::get_size(int32_t& width, int32_t& height) const {
    width = width_;
    height = height_;
}

void linux_window::set_size(int32_t width, int32_t height) {
    width_ = width;
    height_ = height;
    if (display_ && window_) {
        auto* display = display_;
        XResizeWindow(display, window_,
                      static_cast<unsigned int>(width * dpi_scale_),
                      static_cast<unsigned int>(height * dpi_scale_));
        XFlush(display);
    }
    if (renderer_) {
        renderer_->resize(static_cast<uint32_t>(width),
                           static_cast<uint32_t>(height));
    }
    notify_widget_resize();
}

void linux_window::get_position(int32_t& x, int32_t& y) const {
    x = x_;
    y = y_;
}

void linux_window::set_position(int32_t x, int32_t y) {
    x_ = x;
    y_ = y;
    if (display_ && window_) {
        auto* display = display_;
        XMoveWindow(display, window_,
                    static_cast<int>(x * dpi_scale_),
                    static_cast<int>(y * dpi_scale_));
        XFlush(display);
    }
}

bool linux_window::is_visible() const { return is_visible_; }
bool linux_window::is_maximized() const { return is_maximized_; }
bool linux_window::is_minimized() const { return is_minimized_; }
bool linux_window::is_fullscreen() const { return is_fullscreen_; }

void linux_window::set_fullscreen(bool fullscreen) {
    if (fullscreen == is_fullscreen_) return;

    auto* display = display_;

    if (fullscreen) {
        if (!is_maximized_) {
            prev_width_ = width_;
            prev_height_ = height_;
            XWindowAttributes attrs;
            XGetWindowAttributes(display, window_, &attrs);
            prev_x_ = static_cast<int32_t>(attrs.x / dpi_scale_);
            prev_y_ = static_cast<int32_t>(attrs.y / dpi_scale_);
        }

        XEvent xev = {};
        xev.type = ClientMessage;
        xev.xclient.window = window_;
        xev.xclient.message_type = wm_state_atom_;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        xev.xclient.data.l[1] = wm_state_fullscreen_atom_;
        xev.xclient.data.l[2] = 0;
        XSendEvent(display, DefaultRootWindow(display), False,
                   SubstructureNotifyMask, &xev);
    } else {
        XEvent xev = {};
        xev.type = ClientMessage;
        xev.xclient.window = window_;
        xev.xclient.message_type = wm_state_atom_;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
        xev.xclient.data.l[1] = wm_state_fullscreen_atom_;
        xev.xclient.data.l[2] = 0;
        XSendEvent(display, DefaultRootWindow(display), False,
                   SubstructureNotifyMask, &xev);

        if (prev_width_ > 0 && prev_height_ > 0) {
            width_ = prev_width_;
            height_ = prev_height_;
            XMoveResizeWindow(display, window_,
                              static_cast<int>(prev_x_ * dpi_scale_),
                              static_cast<int>(prev_y_ * dpi_scale_),
                              static_cast<unsigned int>(prev_width_ * dpi_scale_),
                              static_cast<unsigned int>(prev_height_ * dpi_scale_));
        }
    }

    is_fullscreen_ = fullscreen;
    XFlush(display);
}

void* linux_window::native_handle() const {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(window_));
}

void linux_window::loop() {
    if (!display_) return;

    bool had_events = process_events();

    if (should_close_) return;

    if (widget_ && renderer_) {
        static auto last_tick = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        float dt_ms = std::chrono::duration<float, std::milli>(now - last_tick).count();
        last_tick = now;
        if (dt_ms > 100.0f) dt_ms = 16.0f;

        widget_->layout();
        widget_->tick(dt_ms);

        renderer_->begin_frame();
        widget_->paint(renderer_);
        renderer_->end_frame();

        glXSwapBuffers(display_, window_);
    } else if (!had_events) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

bool linux_window::should_close() const {
    return should_close_;
}

void* linux_window::user_data() const { return user_data_; }
void linux_window::set_user_data(void* data) { user_data_ = data; }

void linux_window::request_repaint() {
}

void linux_window::set_on_close(void_function callback) { on_close_ = callback; }
void linux_window::set_on_resize(void_function callback) { on_resize_ = callback; }
void linux_window::set_on_key(void_function callback) { on_key_ = callback; }
void linux_window::set_on_mouse(void_function callback) { on_mouse_ = callback; }

void linux_window::set_mouse_capture(bool capture) {
    if (capture) {
        XGrabPointer(display_, window_, True,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    } else {
        XUngrabPointer(display_, CurrentTime);
    }
}

void linux_window::set_widget(std::unique_ptr<widget> widget) {
    widget_ = std::move(widget);
    widget_->set_repaint_callback([this]() {
    });
    widget_->set_window_action_callback([this](int action) {
        switch (action) {
            case widget::action_minimize: minimize(); break;
            case widget::action_maximize:
                if (is_maximized()) restore(); else maximize(); break;
            case widget::action_close: close(); break;
        }
    });
    notify_widget_resize();
}

bool linux_window::initialize(const window_params& params) {
    window_id_ = next_window_id_++;
    dpi_scale_ = 1.0f;
    title_ = params.title;
    width_ = params.width;
    height_ = params.height;

    XSetErrorHandler(x11_error_handler);

    if (!connect_to_x11()) return false;
    if (!select_fb_config()) return false;
    if (!create_x11_window(params)) return false;
    if (!create_gl_context()) return false;
    if (!create_renderer()) return false;

    auto* display = display_;
    XSelectInput(display, window_,
                 ExposureMask | StructureNotifyMask |
                 KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask |
                 FocusChangeMask | VisibilityChangeMask);

    delete_atom_ = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display_, window_, &delete_atom_, 1);

    wm_state_atom_ = XInternAtom(display, "_NET_WM_STATE", False);
    wm_state_fullscreen_atom_ = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    wm_state_max_v_atom_ = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    wm_state_max_h_atom_ = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    net_wm_name_atom_ = XInternAtom(display, "_NET_WM_NAME", False);
    net_wm_icon_atom_ = XInternAtom(display, "_NET_WM_ICON", False);
    utf8_string_atom_ = XInternAtom(display, "UTF8_STRING", False);

    on_close_ = params.on_close;
    on_resize_ = params.on_resize;
    on_key_ = params.on_key;
    on_mouse_ = params.on_mouse;
    user_data_ = params.user_data;

    update_dpi();

    {
        std::string iconPath = platform::executable_directory() + "/res/spiration.png";
        int w = 0, h = 0, channels = 0;
        unsigned char* rgba = stbi_load(iconPath.c_str(), &w, &h, &channels, 4);
        if (rgba) {
            std::vector<unsigned long> icon_data(2 + static_cast<size_t>(w) * h, 0);
            icon_data[0] = static_cast<unsigned long>(w);
            icon_data[1] = static_cast<unsigned long>(h);
            for (int i = 0; i < w * h; ++i) {
                unsigned char r = rgba[i * 4 + 0];
                unsigned char g = rgba[i * 4 + 1];
                unsigned char b = rgba[i * 4 + 2];
                unsigned char a = rgba[i * 4 + 3];
                icon_data[2 + i] = static_cast<unsigned long>(
                    (static_cast<unsigned long>(a) << 24) |
                    (static_cast<unsigned long>(r) << 16) |
                    (static_cast<unsigned long>(g) << 8) |
                    static_cast<unsigned long>(b));
            }
            XChangeProperty(display_, window_, net_wm_icon_atom_, XA_CARDINAL, 32,
                            PropModeReplace,
                            reinterpret_cast<unsigned char*>(icon_data.data()),
                            static_cast<int>(icon_data.size()));
            stbi_image_free(rgba);
        } else {
            console::warning("Failed to load app icon from %s", iconPath.c_str());
        }
    }

    if (params.visible) {
        show();
    }

    initialized_ = true;
    return true;
}

void linux_window::shutdown() {
    if (!initialized_) return;

    if (widget_) {
        widget_.reset();
    }

    if (renderer_) {
        renderer_->shutdown();
        renderer_.reset();
    }

    if (gl_context_) {
        auto* display = display_;
        if (display) {
            glXMakeCurrent(display, None, nullptr);
            glXDestroyContext(display, static_cast<GLXContext>(gl_context_));
        }
        gl_context_ = nullptr;
    }

    if (glx_visual_) {
        XFree(glx_visual_);
        glx_visual_ = nullptr;
    }

    if (window_ && display_) {
        auto* display = display_;
        XDestroyWindow(display, window_);
        window_ = 0;
    }

    if (display_) {
        auto* display = display_;
        XCloseDisplay(display);
        display_ = nullptr;
    }

    initialized_ = false;
}


bool linux_window::connect_to_x11() {
    auto* display = XOpenDisplay(nullptr);
    if (!display) {
        console::error("Failed to open X11 display");
        return false;
    }
    display_ = display;
    screen_number_ = DefaultScreen(display);
    return true;
}


bool linux_window::create_x11_window(const window_params& params) {
    if (!glx_visual_) {
        console::error("create_x11_window: GLX visual not selected");
        return false;
    }

    ::Window root = DefaultRootWindow(display_);

    int width_px = static_cast<int>(params.width * dpi_scale_);
    int height_px = static_cast<int>(params.height * dpi_scale_);

    XSetWindowAttributes swa = {};
    swa.background_pixel = 0;
    swa.border_pixel = 0;
    swa.colormap = XCreateColormap(display_, root, glx_visual_->visual, AllocNone);
    swa.event_mask = StructureNotifyMask;

    window_ = XCreateWindow(display_, root,
                            0, 0, width_px, height_px, 0,
                            glx_visual_->depth, InputOutput,
                            glx_visual_->visual,
                            CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
                            &swa);

    if (!window_) {
        console::error("Failed to create X11 window with GLX visual");
        return false;
    }

    if (!params.title.empty()) {
        XStoreName(display_, window_, params.title.c_str());
        XChangeProperty(display_, window_, net_wm_name_atom_, utf8_string_atom_,
                        8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(params.title.c_str()),
                        static_cast<int>(params.title.size()));
    }

    XSizeHints hints = {};
    hints.flags = PMinSize;
    hints.min_width = static_cast<int>(400 * dpi_scale_);
    hints.min_height = static_cast<int>(300 * dpi_scale_);
    XSetWMNormalHints(display_, window_, &hints);

    if (!params.decorated) {
        Atom motif_hints = XInternAtom(display_, "_MOTIF_WM_HINTS", False);
        if (motif_hints) {
            struct MotifHints {
                unsigned long flags;
                unsigned long functions;
                unsigned long decorations;
                long input_mode;
                unsigned long status;
            };
            // MWM_HINTS_FUNCTIONS=1, MWM_HINTS_DECORATIONS=2
            // MWM_FUNC_MINIMIZE=8, MWM_FUNC_MAXIMIZE=16, MWM_FUNC_CLOSE=32
            MotifHints mh = { 3, 56, 0, 0, 0 };
            XChangeProperty(display_, window_, motif_hints, motif_hints, 32,
                           PropModeReplace, reinterpret_cast<unsigned char*>(&mh), 5);
        }
    }

    Screen* screen = XScreenOfDisplay(display_, screen_number_);
    int screen_w = screen->width;
    int screen_h = screen->height;
    int x = (screen_w - width_px) / 2;
    int y = (screen_h - height_px) / 2;
    x_ = static_cast<int32_t>(x / dpi_scale_);
    y_ = static_cast<int32_t>(y / dpi_scale_);

    XMoveWindow(display_, window_, x, y);

    return true;
}

bool linux_window::select_fb_config() {
    auto* display = display_;
    if (!display) return false;

    static int visual_attrs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, 4,
        None
    };

    int fb_count = 0;
    GLXFBConfig* fb_configs = glXChooseFBConfig(display, screen_number_,
                                                  visual_attrs, &fb_count);
    if (!fb_configs || fb_count == 0) {
        console::error("No suitable GLX framebuffer config found");
        return false;
    }

    glx_fb_config_ = fb_configs[0];
    glx_visual_ = glXGetVisualFromFBConfig(display,
                                            static_cast<GLXFBConfig>(glx_fb_config_));
    XFree(fb_configs);

    if (!glx_visual_) {
        console::error("Failed to get XVisualInfo from GLX FB config");
        return false;
    }

    return true;
}

bool linux_window::create_gl_context() {
    auto* display = display_;
    if (!display || !glx_fb_config_) return false;

    GLXContext shared = nullptr;
    bool direct = True;

    int context_attrs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    using glXCreateContextAttribsARBProc =
        GLXContext (*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
        reinterpret_cast<glXCreateContextAttribsARBProc>(
            glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));

    if (glXCreateContextAttribsARB) {
        gl_context_ = glXCreateContextAttribsARB(display,
                                                  static_cast<GLXFBConfig>(glx_fb_config_),
                                                  shared, direct, context_attrs);
    }

    if (!gl_context_) {
        XVisualInfo* vi = glXGetVisualFromFBConfig(display,
                                                     static_cast<GLXFBConfig>(glx_fb_config_));
        if (!vi) {
            return false;
        }
        gl_context_ = glXCreateContext(display, vi, shared, direct);
        XFree(vi);
    }

    if (!gl_context_) {
        console::error("Failed to create GLX context");
        return false;
    }

    if (!glXMakeCurrent(display, window_, static_cast<GLXContext>(gl_context_))) {
        console::error("Failed to make GLX context current");
        return false;
    }

    return true;
}

bool linux_window::create_renderer() {
    if (renderer_) return true;
    renderer_ = renderer::create_opengl_renderer();
    if (!renderer_) return false;

    if (!renderer_->initialize(display_)) {
        renderer_.reset();
        return false;
    }

    renderer_->resize(static_cast<uint32_t>(width_),
                       static_cast<uint32_t>(height_));
    return true;
}


bool linux_window::process_events() {
    auto* display = display_;
    if (!display) return false;

    bool handled = false;
    while (XPending(display) > 0) {
        handled = true;
        XEvent event;
        XNextEvent(display, &event);

        switch (event.type) {
            case Expose: {
                if (event.xexpose.count == 0) {
                    handle_expose();
                }
                break;
            }
            case ConfigureNotify: {
                XConfigureEvent& ce = event.xconfigure;
                int new_w = static_cast<int>(ce.width / dpi_scale_);
                int new_h = static_cast<int>(ce.height / dpi_scale_);
                x_ = static_cast<int32_t>(ce.x / dpi_scale_);
                y_ = static_cast<int32_t>(ce.y / dpi_scale_);

                if (new_w != width_ || new_h != height_) {
                    handle_resize(static_cast<uint32_t>(new_w),
                                  static_cast<uint32_t>(new_h));
                }
                break;
            }
            case KeyPress: {
                handle_key_press(reinterpret_cast<XKeyEvent*>(&event));
                break;
            }
            case KeyRelease: {
                handle_key_release(reinterpret_cast<XKeyEvent*>(&event));
                break;
            }
            case ButtonPress: {
                XButtonEvent& be = event.xbutton;
                if (be.button == Button4 || be.button == Button5) {
                    handle_mouse_wheel(reinterpret_cast<XButtonEvent*>(&event),
                                       be.button == Button4);
                } else {
                    handle_button_press(reinterpret_cast<XButtonEvent*>(&event));
                }
                break;
            }
            case ButtonRelease: {
                handle_button_release(reinterpret_cast<XButtonEvent*>(&event));
                break;
            }
            case MotionNotify: {
                handle_mouse_motion(reinterpret_cast<XMotionEvent*>(&event));
                break;
            }
            case ClientMessage: {
                if (static_cast<Atom>(event.xclient.data.l[0]) == delete_atom_) {
                    should_close_ = true;
                    if (on_close_) on_close_(this);
                } else if (event.xclient.message_type == wm_state_atom_) {
                    long action = event.xclient.data.l[0];
                    Atom prop1 = static_cast<Atom>(event.xclient.data.l[1]);
                    Atom prop2 = static_cast<Atom>(event.xclient.data.l[2]);
                    auto update_state = [&](Atom prop, bool& flag) {
                        if (prop == None) return;
                    };
                    Atom actual_type;
                    int actual_format;
                    unsigned long nitems, bytes_after;
                    unsigned char* prop_data = nullptr;
                    if (XGetWindowProperty(display_, window_, wm_state_atom_,
                                           0, 1024, False, XA_ATOM,
                                           &actual_type, &actual_format,
                                           &nitems, &bytes_after, &prop_data) == Success) {
                        Atom* atoms = reinterpret_cast<Atom*>(prop_data);
                        is_maximized_ = false;
                        is_fullscreen_ = false;
                        for (unsigned long i = 0; i < nitems; ++i) {
                            if (atoms[i] == wm_state_max_h_atom_ ||
                                atoms[i] == wm_state_max_v_atom_)
                                is_maximized_ = true;
                            if (atoms[i] == wm_state_fullscreen_atom_)
                                is_fullscreen_ = true;
                        }
                        XFree(prop_data);
                    }
                }
                break;
            }
            case DestroyNotify: {
                should_close_ = true;
                if (on_close_) on_close_(this);
                break;
            }
            case FocusIn:
            case FocusOut:
                break;
            case VisibilityNotify: {
                is_visible_ = (event.xvisibility.state != VisibilityFullyObscured);
                break;
            }
            case MapNotify: {
                is_visible_ = true;
                break;
            }
            case UnmapNotify: {
                is_visible_ = false;
                break;
            }
        }
    }
    return handled;
}

void linux_window::handle_expose() {
    if (widget_ && renderer_) {
        renderer_->begin_frame();
        widget_->paint(renderer_);
        renderer_->end_frame();
    }
}

void linux_window::handle_resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;

    width_ = static_cast<int32_t>(width);
    height_ = static_cast<int32_t>(height);

    if (renderer_) {
        renderer_->resize(width, height);
    }

    notify_widget_resize();
    if (widget_) widget_->layout();
    if (on_resize_) on_resize_(this);
}

static int keysym_to_vk(KeySym keysym) {
    switch (keysym) {
        case XK_BackSpace: return 0x08; // VK_BACK
        case XK_Tab:       return 0x09; // VK_TAB
        case XK_Return:    return 0x0D; // VK_RETURN
        case XK_Escape:    return 0x1B; // VK_ESCAPE
        case XK_Delete:    return 0x2E; // VK_DELETE
        case XK_Home:      return 0x24; // VK_HOME
        case XK_Left:      return 0x25; // VK_LEFT
        case XK_Up:        return 0x26; // VK_UP
        case XK_Right:     return 0x27; // VK_RIGHT
        case XK_Down:      return 0x28; // VK_DOWN
        case XK_Prior:     return 0x21; // VK_PRIOR (Page Up)
        case XK_Next:      return 0x22; // VK_NEXT (Page Down)
        case XK_End:       return 0x23; // VK_END
        case XK_F1:        return 0x70; // VK_F1
        case XK_F2:        return 0x71;
        case XK_F3:        return 0x72;
        case XK_F4:        return 0x73;
        case XK_F5:        return 0x74;
        case XK_F6:        return 0x75;
        case XK_F7:        return 0x76;
        case XK_F8:        return 0x77;
        case XK_F9:        return 0x78;
        case XK_F10:       return 0x79;
        case XK_F11:       return 0x7A;
        case XK_F12:       return 0x7B;
        default:
            if (keysym >= ' ' && keysym <= '~') return static_cast<int>(keysym);
            return static_cast<int>(keysym);
    }
}

void linux_window::handle_key_press(XKeyEvent* event) {
    if (!event) return;

    KeySym keysym = XLookupKeysym(event, 0);
    key_state_[keysym] = true;

    if (keysym == XK_F11) {
        set_fullscreen(!is_fullscreen_);
        return;
    }
    if (keysym == XK_Escape && is_fullscreen_) {
        set_fullscreen(false);
        return;
    }

    if (widget_) {
        key_event_data ked;
        ked.key_code = keysym_to_vk(keysym);
        ked.codepoint = 0;
        ked.ctrl = (event->state & ControlMask) != 0;
        ked.shift = (event->state & ShiftMask) != 0;
        ked.alt = (event->state & Mod1Mask) != 0;

        static XIM xim = nullptr;
        static XIC xic = nullptr;
        if (!xim) {
            xim = XOpenIM(display_, nullptr, nullptr, nullptr);
        }
        if (xim && !xic) {
            xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, nullptr);
        }
        if (xic) {
            Status status;
            wchar_t buf[8] = {};
            int len = XwcLookupString(xic, event, buf, 8, &keysym, &status);
            if (len > 0 && status == XLookupChars) {
                ked.codepoint = static_cast<unsigned int>(buf[0]);
            }
        }

        widget_->handle_event(event_type::keyboard, &ked);
    }

    if (on_key_) on_key_(this);
}

void linux_window::handle_key_release(XKeyEvent* event) {
    if (!event) return;
    KeySym keysym = XLookupKeysym(event, 0);
    key_state_[keysym] = false;
    if (on_key_) on_key_(this);
}

int linux_window::hit_test_edge(float x, float y) const {
    if (is_maximized_ || is_fullscreen_) return 0;
    int flags = 0;
    if (x < RESIZE_MARGIN)                flags |= 1; // 左
    if (x > width_ - RESIZE_MARGIN)       flags |= 2; // 右
    if (y < RESIZE_MARGIN)                flags |= 4; // 上
    if (y > height_ - RESIZE_MARGIN)      flags |= 8; // 下
    return flags;
}

static void set_resize_cursor(Display* display, ::Window window, int edge_flags) {
    static const unsigned int shape_map[16] = {
        0, // 无
        XC_sb_h_double_arrow, // 左
        XC_sb_h_double_arrow, // 右
        0, // 左右
        XC_sb_v_double_arrow, // 上
        XC_top_left_corner, // 左上
        XC_top_right_corner,  // 右上
        0, // 上 & 左右
        XC_sb_v_double_arrow, // 下
        XC_bottom_left_corner, // 左下
        XC_bottom_right_corner, // 右下
        0, // 下 & 左右
        0, // 上下
        0, // 上下 & 左
        0, // 上下 & 右
        0, // 全部
    };
    unsigned int shape = 0;
    bool left   = (edge_flags & 1) != 0;
    bool right  = (edge_flags & 2) != 0;
    bool top    = (edge_flags & 4) != 0;
    bool bottom = (edge_flags & 8) != 0;

    if (top && left)        shape = XC_top_left_corner;
    else if (top && right)  shape = XC_top_right_corner;
    else if (bottom && left)  shape = XC_bottom_left_corner;
    else if (bottom && right) shape = XC_bottom_right_corner;
    else if (left || right)   shape = XC_sb_h_double_arrow;
    else if (top || bottom)   shape = XC_sb_v_double_arrow;

    if (shape) {
        Cursor cursor = XCreateFontCursor(display, shape);
        XDefineCursor(display, window, cursor);
        XFreeCursor(display, cursor);
    } else {
        XUndefineCursor(display, window);
    }
}

void linux_window::handle_button_press(XButtonEvent* event) {
    if (!event) return;

    float xDIP = static_cast<float>(event->x) / dpi_scale_;
    float yDIP = static_cast<float>(event->y) / dpi_scale_;

    mouse_button btn = mouse_button::none;
    if (event->button == Button1) btn = mouse_button::left;
    else if (event->button == Button2) btn = mouse_button::middle;
    else if (event->button == Button3) btn = mouse_button::right;

    mouse_event_data data;
    data.position = { xDIP, yDIP };
    data.button = btn;
    data.action = mouse_action::down;
    data.consumed = false;

    if (widget_) {
        widget_->handle_event(event_type::mouse, &data);
    }

    int edge = hit_test_edge(xDIP, yDIP);
    if (btn == mouse_button::left && !data.consumed && edge && !is_maximized_ && !is_fullscreen_) {
        is_resizing_ = true;
        resize_flags_ = edge;
        resize_start_w_ = width_;
        resize_start_h_ = height_;
        resize_start_x_ = x_;
        resize_start_y_ = y_;
        resize_start_mx_ = static_cast<int>(event->x_root);
        resize_start_my_ = static_cast<int>(event->y_root);
    }

    if (btn == mouse_button::left && !data.consumed && !is_resizing_ &&
        yDIP < DRAG_AREA_HEIGHT && !is_fullscreen_) {
        if (is_maximized_) {
            float mx = static_cast<float>(event->x_root) / dpi_scale_;
            float my = static_cast<float>(event->y_root) / dpi_scale_;
            int restore_w = prev_width_ > 0 ? prev_width_ : 800;
            restore();
            int new_x = static_cast<int>(mx - restore_w * 0.5f);
            int new_y = static_cast<int>(my - DRAG_AREA_HEIGHT * 0.5f);
            x_ = new_x;
            y_ = new_y;
            XMoveWindow(display_, window_,
                        static_cast<int>(x_ * dpi_scale_),
                        static_cast<int>(y_ * dpi_scale_));
            XFlush(display_);
            is_dragging_ = true;
            drag_offset_x_ = static_cast<int>(mx - new_x);
            drag_offset_y_ = static_cast<int>(my - new_y);
        } else {
            is_dragging_ = true;
            drag_offset_x_ = static_cast<int>(event->x_root / dpi_scale_ - x_);
            drag_offset_y_ = static_cast<int>(event->y_root / dpi_scale_ - y_);
        }
    }

    if (btn == mouse_button::left && !data.consumed && yDIP < DRAG_AREA_HEIGHT) {
        if (event->time - last_click_time_ < 400 &&
            std::abs(xDIP - last_click_x_) < 5 &&
            std::abs(yDIP - last_click_y_) < 5) {
            if (is_maximized_) restore(); else maximize();
        }
        last_click_time_ = event->time;
        last_click_x_ = xDIP;
        last_click_y_ = yDIP;
    }

    if (on_mouse_) on_mouse_(this);
}

void linux_window::handle_button_release(XButtonEvent* event) {
    if (!event) return;

    is_resizing_ = false;
    is_dragging_ = false;

    float xDIP = static_cast<float>(event->x) / dpi_scale_;
    float yDIP = static_cast<float>(event->y) / dpi_scale_;

    mouse_button btn = mouse_button::none;
    if (event->button == Button1) btn = mouse_button::left;
    else if (event->button == Button2) btn = mouse_button::middle;
    else if (event->button == Button3) btn = mouse_button::right;

    mouse_event_data data;
    data.position = { xDIP, yDIP };
    data.button = btn;
    data.action = mouse_action::up;
    data.consumed = false;

    if (widget_) {
        widget_->handle_event(event_type::mouse, &data);
    }
    if (on_mouse_) on_mouse_(this);
}

void linux_window::handle_mouse_motion(XMotionEvent* event) {
    if (!event) return;

    float xDIP = static_cast<float>(event->x) / dpi_scale_;
    float yDIP = static_cast<float>(event->y) / dpi_scale_;
    last_mouse_x_ = static_cast<int32_t>(event->x);
    last_mouse_y_ = static_cast<int32_t>(event->y);

    if (is_resizing_) {
        int dx = static_cast<int>(static_cast<float>(event->x_root) / dpi_scale_ -
                                   static_cast<float>(resize_start_mx_) / dpi_scale_);
        int dy = static_cast<int>(static_cast<float>(event->y_root) / dpi_scale_ -
                                   static_cast<float>(resize_start_my_) / dpi_scale_);
        int new_x = resize_start_x_;
        int new_y = resize_start_y_;
        int new_w = resize_start_w_;
        int new_h = resize_start_h_;

        if (resize_flags_ & 1) { new_x = resize_start_x_ + dx; new_w = resize_start_w_ - dx; }
        if (resize_flags_ & 2) { new_w = resize_start_w_ + dx; }
        if (resize_flags_ & 4) { new_y = resize_start_y_ + dy; new_h = resize_start_h_ - dy; }
        if (resize_flags_ & 8) { new_h = resize_start_h_ + dy; }

        int min_w = 400, min_h = 300;
        if (new_w < min_w) {
            if (resize_flags_ & 1) new_x = resize_start_x_ + resize_start_w_ - min_w;
            new_w = min_w;
        }
        if (new_h < min_h) {
            if (resize_flags_ & 4) new_y = resize_start_y_ + resize_start_h_ - min_h;
            new_h = min_h;
        }

        x_ = new_x;
        y_ = new_y;
        width_ = new_w;
        height_ = new_h;

        XMoveResizeWindow(display_, window_,
                          static_cast<int>(x_ * dpi_scale_),
                          static_cast<int>(y_ * dpi_scale_),
                          static_cast<unsigned int>(new_w * dpi_scale_),
                          static_cast<unsigned int>(new_h * dpi_scale_));
        XFlush(display_);

        if (renderer_) {
            renderer_->resize(static_cast<uint32_t>(new_w),
                               static_cast<uint32_t>(new_h));
        }
        notify_widget_resize();
        return;
    }

    if (is_dragging_) {
        int new_x = static_cast<int>(event->x_root / dpi_scale_ - drag_offset_x_);
        int new_y = static_cast<int>(event->y_root / dpi_scale_ - drag_offset_y_);
        if (new_x != x_ || new_y != y_) {
            x_ = new_x;
            y_ = new_y;
            XMoveWindow(display_, window_,
                        static_cast<int>(x_ * dpi_scale_),
                        static_cast<int>(y_ * dpi_scale_));
        }
        return;
    }

    if (!is_maximized_ && !is_fullscreen_) {
        int edge = hit_test_edge(xDIP, yDIP);
        if (edge && yDIP >= DRAG_AREA_HEIGHT) {
            set_resize_cursor(display_, window_, edge);
        } else {
            XUndefineCursor(display_, window_);
        }
    }

    mouse_event_data data;
    data.position = { xDIP, yDIP };
    data.button = mouse_button::none;
    data.action = mouse_action::move;
    data.consumed = false;

    if (widget_) {
        widget_->handle_event(event_type::mouse, &data);
    }
    if (on_mouse_) on_mouse_(this);
}

void linux_window::handle_mouse_wheel(XButtonEvent* event, bool up) {
    if (!event) return;

    float xDIP = static_cast<float>(event->x) / dpi_scale_;
    float yDIP = static_cast<float>(event->y) / dpi_scale_;

    mouse_event_data data;
    data.position = { xDIP, yDIP };
    data.button = mouse_button::none;
    data.action = mouse_action::wheel;
    data.wheel_delta = up ? 120 : -120;
    data.consumed = false;

    if (widget_) {
        widget_->handle_event(event_type::mouse, &data);
    }
    if (on_mouse_) on_mouse_(this);
}


void linux_window::update_dpi() {
    auto* display = display_;
    if (!display) {
        dpi_scale_ = 1.0f;
        return;
    }

    Screen* screen = XScreenOfDisplay(display, screen_number_);
    if (screen) {
        int width_mm = screen->mwidth;
        int width_px = screen->width;
        if (width_mm > 0) {
            float dpi = static_cast<float>(width_px) * 25.4f / static_cast<float>(width_mm);
            dpi_scale_ = dpi / 96.0f;
        }
    }

    if (dpi_scale_ < 1.0f) dpi_scale_ = 1.0f;
}

void linux_window::notify_widget_resize() {
    if (!widget_) return;

    size sizeData = { static_cast<float>(width_), static_cast<float>(height_) };
    widget_->handle_event(event_type::window_resize, &sizeData);
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

} // namespace spiration







