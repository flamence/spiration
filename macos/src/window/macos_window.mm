/**
 * @file macos_window.mm
 * @brief macOS 平台窗口实现。
 * @author clk
 */

#import <window/macos_window.h>
#import <renderer/metal_renderer.h>
#import <ui/point.h>
#import <ui/size.h>
#import <utils/console.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <string>
#import <cmath>

namespace spiration {

uint32_t Window::s_NextWindowId = 1;

@interface SpirationWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) spiration::Window* spirationWindow;
@end

@implementation SpirationWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
    if (self.spirationWindow) {
        self.spirationWindow->close();
    }
}

- (void)windowDidResize:(NSNotification*)notification {
    if (self.spirationWindow) {
        NSWindow* win = (NSWindow*)self.spirationWindow->native_handle();
        if (!win) return;

        NSView* contentView = [win contentView];
        CGSize size = contentView.bounds.size;
        CGFloat scale = [contentView.window backingScaleFactor];

        CAMetalLayer* metalLayer = (CAMetalLayer*)contentView.layer;
        if (metalLayer) {
            metalLayer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
        }

        int32_t w = static_cast<int32_t>(size.width);
        int32_t h = static_cast<int32_t>(size.height);
        if (w != self.spirationWindow->m_Width || h != self.spirationWindow->m_Height) {
            self.spirationWindow->m_Width = w;
            self.spirationWindow->m_Height = h;
            self.spirationWindow->m_BackingScale = static_cast<float>(scale);

            if (self.spirationWindow->m_Renderer) {
                self.spirationWindow->m_Renderer->resize(
                    static_cast<uint32_t>(w * scale),
                    static_cast<uint32_t>(h * scale));
            }

            self.spirationWindow->notify_widget_resize();
            if (self.spirationWindow->m_OnResize) {
                self.spirationWindow->m_OnResize(self.spirationWindow);
            }
        }
    }
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
}

- (void)windowDidResignKey:(NSNotification*)notification {
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    return YES;
}

@end

Window::~Window() {
    shutdown();
}

Window::Window(Window&& other) noexcept
    : m_NSWindow(other.m_NSWindow)
    , m_NSView(other.m_NSView)
    , m_MetalLayer(other.m_MetalLayer)
    , m_Title(std::move(other.m_Title))
    , m_Width(other.m_Width)
    , m_Height(other.m_Height)
    , m_X(other.m_X)
    , m_Y(other.m_Y)
    , m_BackingScale(other.m_BackingScale)
    , m_ShouldClose(other.m_ShouldClose)
    , m_IsMaximized(other.m_IsMaximized)
    , m_IsMinimized(other.m_IsMinimized)
    , m_IsFullscreen(other.m_IsFullscreen)
    , m_IsVisible(other.m_IsVisible)
    , m_Initialized(other.m_Initialized)
    , m_OnClose(std::move(other.m_OnClose))
    , m_OnResize(std::move(other.m_OnResize))
    , m_OnKey(std::move(other.m_OnKey))
    , m_OnMouse(std::move(other.m_OnMouse))
    , m_Renderer(std::move(other.m_Renderer))
    , m_Widget(std::move(other.m_Widget))
    , m_UserData(other.m_UserData)
    , m_WindowId(other.m_WindowId)
{
    other.m_NSWindow = nil;
    other.m_NSView = nil;
    other.m_MetalLayer = nil;
    other.m_ShouldClose = false;
    other.m_IsVisible = false;
    other.m_Initialized = false;
    other.m_UserData = nullptr;
    other.m_WindowId = 0;
    other.m_BackingScale = 2.0f;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        shutdown();

        m_NSWindow = other.m_NSWindow;
        m_NSView = other.m_NSView;
        m_MetalLayer = other.m_MetalLayer;
        m_Title = std::move(other.m_Title);
        m_Width = other.m_Width;
        m_Height = other.m_Height;
        m_X = other.m_X;
        m_Y = other.m_Y;
        m_BackingScale = other.m_BackingScale;
        m_ShouldClose = other.m_ShouldClose;
        m_IsMaximized = other.m_IsMaximized;
        m_IsMinimized = other.m_IsMinimized;
        m_IsFullscreen = other.m_IsFullscreen;
        m_IsVisible = other.m_IsVisible;
        m_Initialized = other.m_Initialized;
        m_OnClose = std::move(other.m_OnClose);
        m_OnResize = std::move(other.m_OnResize);
        m_OnKey = std::move(other.m_OnKey);
        m_OnMouse = std::move(other.m_OnMouse);
        m_Renderer = std::move(other.m_Renderer);
        m_Widget = std::move(other.m_Widget);
        m_UserData = other.m_UserData;
        m_WindowId = other.m_WindowId;

        other.m_NSWindow = nil;
        other.m_NSView = nil;
        other.m_MetalLayer = nil;
        other.m_ShouldClose = false;
        other.m_IsVisible = false;
        other.m_Initialized = false;
        other.m_UserData = nullptr;
        other.m_WindowId = 0;
        other.m_BackingScale = 2.0f;
    }
    return *this;
}

void Window::show() {
    @autoreleasepool {
        [m_NSWindow makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        m_IsVisible = YES;
    }
}

void Window::hide() {
    @autoreleasepool {
        [m_NSWindow orderOut:nil];
        m_IsVisible = NO;
    }
}

void Window::maximize() {
    @autoreleasepool {
        [m_NSWindow zoom:nil];
        m_IsMaximized = YES;
    }
}

void Window::minimize() {
    @autoreleasepool {
        [m_NSWindow miniaturize:nil];
        m_IsMinimized = YES;
    }
}

void Window::restore() {
    @autoreleasepool {
        if (m_IsMinimized) {
            [m_NSWindow deminiaturize:nil];
            m_IsMinimized = NO;
        } else if (m_IsMaximized) {
            [m_NSWindow zoom:nil];
            m_IsMaximized = NO;
        }
        m_IsVisible = YES;
    }
}

void Window::close() {
    m_ShouldClose = YES;
    @autoreleasepool {
        [m_NSWindow close];
    }
    if (m_OnClose) m_OnClose(this);
}

std::string Window::title() const {
    return m_Title;
}

void Window::set_title(const std::string& title) {
    m_Title = title;
    @autoreleasepool {
        [m_NSWindow setTitle:[NSString stringWithUTF8String:title.c_str()]];
    }
}

void Window::get_size(int32_t& width, int32_t& height) const {
    width = m_Width;
    height = m_Height;
}

void Window::set_size(int32_t width, int32_t height) {
    m_Width = width;
    m_Height = height;
    @autoreleasepool {
        NSRect frame = [m_NSWindow frame];
        frame.size.width = static_cast<CGFloat>(width);
        frame.size.height = static_cast<CGFloat>(height);
        [m_NSWindow setFrame:frame display:YES animate:NO];

        CAMetalLayer* layer = (CAMetalLayer*)m_NSView.layer;
        if (layer) {
            CGFloat scale = [m_NSWindow backingScaleFactor];
            layer.drawableSize = CGSizeMake(width * scale, height * scale);
            m_BackingScale = static_cast<float>(scale);
        }
    }
    if (m_Renderer) {
        m_Renderer->resize(static_cast<uint32_t>(width * m_BackingScale),
                           static_cast<uint32_t>(height * m_BackingScale));
    }
    notify_widget_resize();
}

void Window::get_position(int32_t& x, int32_t& y) const {
    x = m_X;
    y = m_Y;
}

void Window::set_position(int32_t x, int32_t y) {
    m_X = x;
    m_Y = y;
    @autoreleasepool {
        NSRect frame = [m_NSWindow frame];
        NSScreen* screen = [m_NSWindow screen] ?: [NSScreen mainScreen];
        CGFloat screenH = screen.frame.size.height;
        frame.origin.x = static_cast<CGFloat>(x);
        frame.origin.y = screenH - frame.size.height - static_cast<CGFloat>(y);
        [m_NSWindow setFrameOrigin:frame.origin];
    }
}

bool Window::is_visible() const { return m_IsVisible; }
bool Window::is_maximized() const {
    return [m_NSWindow isZoomed];
}
bool Window::is_minimized() const { return m_IsMinimized; }
bool Window::is_fullscreen() const { return m_IsFullscreen; }

void Window::set_fullscreen(bool fullscreen) {
    @autoreleasepool {
        if (fullscreen != m_IsFullscreen) {
            [m_NSWindow toggleFullScreen:nil];
            m_IsFullscreen = fullscreen;
        }
    }
}

void* Window::native_handle() const {
    return (__bridge void*)m_NSWindow;
}

void Window::loop() {
    @autoreleasepool {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }

        if (!m_ShouldClose && m_Widget && m_Renderer) {
            m_Renderer->begin_frame();
            m_Widget->paint(m_Renderer.get());
            m_Renderer->end_frame();
        }
    }
}

bool Window::should_close() const {
    return m_ShouldClose;
}

void* Window::user_data() const { return m_UserData; }
void Window::set_user_data(void* data) { m_UserData = data; }

void Window::request_repaint() {
}

void Window::set_on_close(void_function callback) { m_OnClose = callback; }
void Window::set_on_resize(void_function callback) { m_OnResize = callback; }
void Window::set_on_key(void_function callback) { m_OnKey = callback; }
void Window::set_on_mouse(void_function callback) { m_OnMouse = callback; }

void Window::set_mouse_capture(bool capture) {
    (void)capture;
}

void Window::set_widget(std::unique_ptr<widget> widget) {
    m_Widget = std::move(widget);
    notify_widget_resize();
}

bool Window::initialize(const window_params& params) {
    m_WindowId = s_NextWindowId++;
    m_Title = params.title;
    m_Width = params.width;
    m_Height = params.height;

    @autoreleasepool {
        if (!create_cocoa_window(params)) return false;
    }

    if (!create_renderer()) return false;

    m_OnClose = params.on_close;
    m_OnResize = params.on_resize;
    m_OnKey = params.on_key;
    m_OnMouse = params.on_mouse;
    m_UserData = params.user_data;

    m_Initialized = true;

    if (params.visible) {
        show();
    }

    return true;
}

void Window::shutdown() {
    if (!m_Initialized) return;

    if (m_Widget) {
        m_Widget.reset();
    }

    if (m_Renderer) {
        m_Renderer->shutdown();
        m_Renderer.reset();
    }

    @autoreleasepool {
        if (m_NSWindow) {
            [m_NSWindow close];
            m_NSWindow = nil;
        }
        m_NSView = nil;
        m_MetalLayer = nil;
    }

    m_Initialized = false;
}

bool Window::create_cocoa_window(const window_params& params) {
    @autoreleasepool {
        if (!NSApp) {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        }

        NSWindowStyleMask styleMask = NSWindowStyleMaskTitled |
                                       NSWindowStyleMaskClosable |
                                       NSWindowStyleMaskMiniaturizable |
                                       NSWindowStyleMaskResizable;

        if (!params.decorated) {
            styleMask = NSWindowStyleMaskBorderless;
        }

        NSRect contentRect = NSMakeRect(0, 0, params.width, params.height);
        m_NSWindow = [[NSWindow alloc] initWithContentRect:contentRect
                                                  styleMask:styleMask
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];

        if (!m_NSWindow) return false;

        [m_NSWindow setTitle:[NSString stringWithUTF8String:params.title.c_str()]];
        [m_NSWindow setAcceptsMouseMovedEvents:YES];
        [m_NSWindow setRestorable:NO];

        SpirationWindowDelegate* delegate = [[SpirationWindowDelegate alloc] init];
        delegate.spirationWindow = this;
        [m_NSWindow setDelegate:delegate];

        NSRect viewRect = NSMakeRect(0, 0, params.width, params.height);
        m_NSView = [[NSView alloc] initWithFrame:viewRect];
        [m_NSView setWantsLayer:YES];

        m_MetalLayer = [CAMetalLayer layer];
        m_MetalLayer.device = MTLCreateSystemDefaultDevice();
        m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        m_MetalLayer.framebufferOnly = YES;
        m_MetalLayer.frame = viewRect;

        CGFloat scale = [m_NSWindow backingScaleFactor];
        m_MetalLayer.drawableSize = CGSizeMake(params.width * scale,
                                                params.height * scale);
        m_BackingScale = static_cast<float>(scale);

        [m_NSView setLayer:m_MetalLayer];
        [m_NSWindow setContentView:m_NSView];

        [m_NSWindow center];

        NSRect frame = [m_NSWindow frame];
        m_X = static_cast<int32_t>(frame.origin.x);
        m_Y = static_cast<int32_t>(frame.origin.y);
        m_Width = static_cast<int32_t>(frame.size.width);
        m_Height = static_cast<int32_t>(frame.size.height);

        return true;
    }
}

bool Window::create_renderer() {
    if (m_Renderer) return true;
    m_Renderer = renderer::create_metal_renderer();
    if (!m_Renderer) return false;

    if (!m_Renderer->initialize((__bridge void*)m_MetalLayer)) {
        m_Renderer.reset();
        return false;
    }

    CGFloat scale = [m_NSWindow backingScaleFactor];
    m_Renderer->resize(static_cast<uint32_t>(m_Width * scale),
                       static_cast<uint32_t>(m_Height * scale));
    return true;
}

void Window::notify_widget_resize() {
    if (!m_Widget) return;
    size sizeData = { static_cast<float>(m_Width), static_cast<float>(m_Height) };
    m_Widget->handle_event(event_type::window_resize, &sizeData);
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
