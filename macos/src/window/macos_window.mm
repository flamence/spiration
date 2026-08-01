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

using namespace spiration;

static constexpr float DRAG_AREA_HEIGHT = 34.0f;
static constexpr float RESIZE_MARGIN = 6.0f;
static constexpr float MIN_WINDOW_WIDTH = 400.0f;
static constexpr float MIN_WINDOW_HEIGHT = 300.0f;

static int cocoa_key_to_vk(unsigned short keyCode) {
    switch (keyCode) {
        case 0x33: return 0x08; // VK_BACK
        case 0x30: return 0x09; // VK_TAB
        case 0x24: return 0x0D; // VK_RETURN
        case 0x1B: return 0x1B; // VK_ESCAPE
        case 0x75: return 0x2E; // VK_DELETE
        case 0x73: return 0x24; // VK_HOME
        case 0x7B: return 0x25; // VK_LEFT
        case 0x7E: return 0x26; // VK_UP
        case 0x7C: return 0x27; // VK_RIGHT
        case 0x7D: return 0x28; // VK_DOWN
        case 0x74: return 0x21; // VK_PRIOR
        case 0x79: return 0x22; // VK_NEXT
        case 0x77: return 0x23; // VK_END
        case 0x7A: return 0x70; // VK_F1
        case 0x78: return 0x71; // VK_F2
        case 0x63: return 0x72; // VK_F3
        case 0x76: return 0x73; // VK_F4
        case 0x60: return 0x74; // VK_F5
        case 0x61: return 0x75; // VK_F6
        case 0x62: return 0x76; // VK_F7
        case 0x64: return 0x77; // VK_F8
        case 0x65: return 0x78; // VK_F9
        case 0x6D: return 0x79; // VK_F10
        case 0x67: return 0x7A; // VK_F11
        case 0x6F: return 0x7B; // VK_F12
        default: return static_cast<int>(keyCode);
    }
}

@interface SpirationContentView : NSView
@property (nonatomic, assign) spiration::Window* spirationWindow;
@end

@implementation SpirationContentView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea* area in self.trackingAreas) {
        [self removeTrackingArea:area];
    }
    NSTrackingArea* area = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                         options:NSTrackingMouseMoved | NSTrackingActiveInActiveApp | NSTrackingInVisibleRect
                                                           owner:self userInfo:nil];
    [self addTrackingArea:area];
}

- (int)hitTestEdge:(NSPoint)loc {
    spiration::Window* win = self.spirationWindow;
    if (!win) return 0;
    int32_t w, h;
    win->get_size(w, h);
    int flags = 0;
    if (loc.x < RESIZE_MARGIN)                flags |= 1; // 左
    if (loc.x > w - RESIZE_MARGIN)            flags |= 2; // 右
    if (loc.y < RESIZE_MARGIN)                flags |= 4; // 下
    if (loc.y > h - RESIZE_MARGIN)            flags |= 8; // 上
    return flags;
}

- (void)updateResizeCursor:(int)edge {
    if (edge == 0) {
        [[NSCursor arrowCursor] set];
        return;
    }
    BOOL left   = (edge & 1) != 0;
    BOOL right  = (edge & 2) != 0;
    BOOL bottom = (edge & 4) != 0;
    BOOL top    = (edge & 8) != 0;

    if (left || right) { [[NSCursor resizeLeftRightCursor] set]; return; }
    if (bottom || top) { [[NSCursor resizeUpDownCursor] set]; return; }
}

- (void)mouseDown:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;

    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];

    int edge = [self hitTestEdge:loc];
    if (edge != 0) {
        win->m_IsResizing = true;
        win->m_ResizeEdge = edge;
        win->m_DragStartMouseX = event.locationInWindow.x;
        win->m_DragStartMouseY = event.locationInWindow.y;
        NSRect frame = [win->get_ns_window() frame];
        win->m_DragStartX = frame.origin.x;
        win->m_DragStartY = frame.origin.y;
        win->m_DragStartW = frame.size.width;
        win->m_DragStartH = frame.size.height;
        return;
    }

    int32_t w, h;
    win->get_size(w, h);
    if (loc.y > h - DRAG_AREA_HEIGHT) {
        if (event.clickCount == 2) {
            if (win->is_maximized()) {
                win->restore();
            } else {
                win->maximize();
            }
            return;
        }
        win->m_IsDragging = true;
        win->m_DragStartMouseX = event.locationInWindow.x;
        win->m_DragStartMouseY = event.locationInWindow.y;
        NSRect frame = [win->get_ns_window() frame];
        win->m_DragStartX = frame.origin.x;
        win->m_DragStartY = frame.origin.y;
        return;
    }

    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(loc.y) };
    data.button = mouse_button::left;
    data.action = mouse_action::down;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
    if (win->on_mouse()) win->on_mouse()(win);
}

- (void)mouseUp:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;
    win->m_IsDragging = false;
    win->m_IsResizing = false;

    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(loc.y) };
    data.button = mouse_button::left;
    data.action = mouse_action::up;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
    if (win->on_mouse()) win->on_mouse()(win);
}

- (void)rightMouseDown:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(loc.y) };
    data.button = mouse_button::right;
    data.action = mouse_action::down;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
    if (win->on_mouse()) win->on_mouse()(win);
}

- (void)rightMouseUp:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(loc.y) };
    data.button = mouse_button::right;
    data.action = mouse_action::up;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
    if (win->on_mouse()) win->on_mouse()(win);
}

- (void)mouseMoved:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];

    if (!win->m_IsResizing) {
        int edge = [self hitTestEdge:loc];
        [self updateResizeCursor:edge];
    }

    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(loc.y) };
    data.action = mouse_action::move;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
    if (win->on_mouse()) win->on_mouse()(win);
}

- (void)mouseDragged:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;

    if (win->m_IsResizing) {
        NSRect frame = NSMakeRect(win->m_DragStartX, win->m_DragStartY,
                                   win->m_DragStartW, win->m_DragStartH);
        CGFloat dx = event.locationInWindow.x - win->m_DragStartMouseX;
        CGFloat dy = event.locationInWindow.y - win->m_DragStartMouseY;

        if (win->m_ResizeEdge & 1) { // 左
            frame.origin.x += dx;
            frame.size.width -= dx;
            if (frame.size.width < MIN_WINDOW_WIDTH) frame.size.width = MIN_WINDOW_WIDTH;
        }
        if (win->m_ResizeEdge & 2) { // 右
            frame.size.width += dx;
            if (frame.size.width < MIN_WINDOW_WIDTH) frame.size.width = MIN_WINDOW_WIDTH;
        }
        if (win->m_ResizeEdge & 4) { // 下
            frame.origin.y += dy;
            frame.size.height -= dy;
            if (frame.size.height < MIN_WINDOW_HEIGHT) frame.size.height = MIN_WINDOW_HEIGHT;
        }
        if (win->m_ResizeEdge & 8) { // 上
            frame.size.height += dy;
            if (frame.size.height < MIN_WINDOW_HEIGHT) frame.size.height = MIN_WINDOW_HEIGHT;
        }

        [win->get_ns_window() setFrame:frame display:YES animate:NO];
        return;
    }

    if (win->m_IsDragging) {
        NSRect frame = [win->get_ns_window() frame];
        NSPoint origin = frame.origin;
        origin.x += event.locationInWindow.x - win->m_DragStartMouseX;
        origin.y += event.locationInWindow.y - win->m_DragStartMouseY;
        [win->get_ns_window() setFrameOrigin:origin];
        return;
    }

    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(loc.y) };
    data.action = mouse_action::move;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
    if (win->on_mouse()) win->on_mouse()(win);
}

- (void)scrollWheel:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(loc.y) };
    data.action = mouse_action::wheel;
    data.wheel_delta = static_cast<int>([event scrollingDeltaY] * 10);
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
    if (win->on_mouse()) win->on_mouse()(win);
}

- (void)keyDown:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;

    unsigned short keyCode = [event keyCode];
    int vk = cocoa_key_to_vk(keyCode);

    if (vk == 0x7A) {
        win->set_fullscreen(!win->is_fullscreen());
        return;
    }
    if (vk == 0x1B && win->is_fullscreen()) {
        win->set_fullscreen(false);
        return;
    }

    key_event_data ked;
    ked.key_code = vk;
    ked.codepoint = 0;
    NSEventModifierFlags mods = [event modifierFlags];
    ked.ctrl = (mods & NSEventModifierFlagControl) != 0;
    ked.shift = (mods & NSEventModifierFlagShift) != 0;
    ked.alt = (mods & NSEventModifierFlagOption) != 0;

    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::keyboard, &ked);
    }
    if (win->on_key()) win->on_key()(win);

    NSString* chars = [event characters];
    if ([chars length] > 0) {
        key_event_data ced;
        ced.codepoint = static_cast<unsigned int>([chars characterAtIndex:0]);
        ced.ctrl = ked.ctrl;
        ced.shift = ked.shift;
        ced.alt = ked.alt;
        if (win->get_widget()) {
            win->get_widget()->handle_event(event_type::keyboard, &ced);
        }
    }
}

- (void)keyUp:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::Window* win = self.spirationWindow;

    key_event_data ked;
    ked.key_code = cocoa_key_to_vk([event keyCode]);
    NSEventModifierFlags mods = [event modifierFlags];
    ked.ctrl = (mods & NSEventModifierFlagControl) != 0;
    ked.shift = (mods & NSEventModifierFlagShift) != 0;
    ked.alt = (mods & NSEventModifierFlagOption) != 0;

    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::keyboard, &ked);
    }
    if (win->on_key()) win->on_key()(win);
}

@end

@interface SpirationWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) spiration::Window* spirationWindow;
@end

@implementation SpirationWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
    if (self.spirationWindow) {
        self.spirationWindow->set_should_close(true);
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

            if (self.spirationWindow->get_renderer()) {
                self.spirationWindow->get_renderer()->resize(
                    static_cast<uint32_t>(w * scale),
                    static_cast<uint32_t>(h * scale));
            }

            self.spirationWindow->notify_widget_resize_public();
            if (self.spirationWindow->on_resize()) {
                self.spirationWindow->on_resize()(self.spirationWindow);
            }
        }
    }
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    if (self.spirationWindow && self.spirationWindow->get_ns_view()) {
        NSView* view = self.spirationWindow->get_ns_view();
        [view.window makeFirstResponder:view];
    }
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    return YES;
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
    if (self.spirationWindow) {
        self.spirationWindow->set_fullscreen_state(true);
    }
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
    if (self.spirationWindow) {
        self.spirationWindow->set_fullscreen_state(false);
    }
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
    if (self.spirationWindow) {
        self.spirationWindow->set_minimized_state(true);
    }
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
    if (self.spirationWindow) {
        self.spirationWindow->set_minimized_state(false);
    }
}

@end

namespace spiration {

uint32_t Window::s_NextWindowId = 1;

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
        m_IsVisible = true;
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
    if (m_ShouldClose) return;
    m_ShouldClose = YES;
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
                                            untilDate:[NSDate dateWithTimeIntervalSinceNow:0.016]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }

        if (!m_ShouldClose && m_Widget && m_Renderer) {
            if (m_NeedsLayout) {
                m_Widget->layout();
                m_NeedsLayout = false;
            }
            m_Renderer->begin_frame();
            m_Widget->paint(m_Renderer);
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
    @autoreleasepool {
        NSEvent* dummy = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:[m_NSWindow windowNumber]
                                             context:nil
                                             subtype:0
                                               data1:0
                                               data2:0];
        [NSApp postEvent:dummy atStart:NO];
    }
}

void Window::request_layout() {
    m_NeedsLayout = true;
}

void Window::set_cursor(cursor_type c) {
    @autoreleasepool {
        NSCursor* cur = [NSCursor arrowCursor];
        switch (c) {
        case cursor_type::text:        cur = [NSCursor IBeamCursor]; break;
        case cursor_type::pointer:     cur = [NSCursor pointingHandCursor]; break;
        case cursor_type::crosshair:   cur = [NSCursor crosshairCursor]; break;
        case cursor_type::move:        cur = [NSCursor openHandCursor]; break;
        case cursor_type::resize_h:    cur = [NSCursor resizeLeftRightCursor]; break;
        case cursor_type::resize_v:    cur = [NSCursor resizeUpDownCursor]; break;
        case cursor_type::resize_nwse: cur = [NSCursor closedHandCursor]; break;
        case cursor_type::resize_nesw: cur = [NSCursor closedHandCursor]; break;
        case cursor_type::forbidden:   cur = [NSCursor operationNotAllowedCursor]; break;
        case cursor_type::default_cursor:
        default:                       break;
        }
        [cur set];
    }
}

void Window::set_on_close(void_function callback) { m_OnClose = callback; }
void Window::set_on_resize(void_function callback) { m_OnResize = callback; }
void Window::set_on_key(void_function callback) { m_OnKey = callback; }
void Window::set_on_mouse(void_function callback) { m_OnMouse = callback; }

void Window::set_mouse_capture(bool capture) {
    if (capture) {
        [NSCursor hide];
        CGAssociateMouseAndMouseCursorPosition(false);
    } else {
        CGAssociateMouseAndMouseCursorPosition(true);
        [NSCursor unhide];
    }
}

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
    notify_widget_resize();
    request_repaint();
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
        SpirationContentView* contentView = [[SpirationContentView alloc] initWithFrame:viewRect];
        contentView.spirationWindow = this;
        contentView.wantsLayer = YES;
        m_NSView = contentView;

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
        [m_NSWindow makeFirstResponder:m_NSView];

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
    m_NeedsLayout = true;
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
