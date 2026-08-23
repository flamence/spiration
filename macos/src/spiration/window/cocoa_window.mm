/**
 * @file cocoa_window.mm
 * @brief 窗口实现。
 * @author clk
 */

#include <cmath>
#include <spiration/ui/point.h>
#include <spiration/ui/size.h>
#include <spiration/window/ime.h>
#include <spiration/window/virtual_key.h>
#include <string>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <spiration/renderer/metal_renderer.h>
#import <spiration/renderer/opengl_platform.h>
#import <spiration/window/cocoa_window.h>

using namespace spiration;

static constexpr float DRAG_AREA_HEIGHT = spiration::kTitleBarDragHeight;
static constexpr float RESIZE_MARGIN = spiration::kResizeBorderWidth;
static constexpr float MIN_WINDOW_WIDTH = spiration::kMinWindowWidth;
static constexpr float MIN_WINDOW_HEIGHT = spiration::kMinWindowHeight;

static int cocoa_key_to_vk(unsigned short keyCode) {
    // 特殊键：macOS keyCode → Windows VK（与 common/vk 一致）。
    switch (keyCode) {
        case 0x33: return vk::Back;
        case 0x30: return vk::Tab;
        case 0x24: return vk::Return;
        case 0x35: return vk::Escape;
        case 0x75: return vk::Delete;
        case 0x73: return vk::Home;
        case 0x7B: return vk::Left;
        case 0x7E: return vk::Up;
        case 0x7C: return vk::Right;
        case 0x7D: return vk::Down;
        case 0x74: return vk::PageUp;
        case 0x79: return vk::PageDown;
        case 0x77: return vk::End;
        case 0x7A: return vk::F1;
        case 0x78: return vk::F2;
        case 0x63: return vk::F3;
        case 0x76: return vk::F4;
        case 0x60: return vk::F5;
        case 0x61: return vk::F6;
        case 0x62: return vk::F7;
        case 0x64: return vk::F8;
        case 0x65: return vk::F9;
        case 0x6D: return vk::F10;
        case 0x67: return vk::F11;
        case 0x6F: return vk::F12;
    }
    // ANSI 字符键：macOS keyCode → VK。注意不可返回原始 keyCode：
    // 它会被当作 VK 使用（如 'W'=0x0D 撞 vk::Return=0x0D，导致输入 W 触发回车）。
    switch (keyCode) {
        case 0x00: return 0x41; // A
        case 0x01: return 0x53; // S
        case 0x02: return 0x44; // D
        case 0x03: return 0x46; // F
        case 0x04: return 0x48; // H
        case 0x05: return 0x47; // G
        case 0x06: return 0x5A; // Z
        case 0x07: return 0x58; // X
        case 0x08: return 0x43; // C
        case 0x09: return 0x56; // V
        case 0x0B: return 0x42; // B
        case 0x0C: return 0x51; // Q
        case 0x0D: return 0x57; // W
        case 0x0E: return 0x45; // E
        case 0x0F: return 0x52; // R
        case 0x10: return 0x59; // Y
        case 0x11: return 0x54; // T
        case 0x12: return 0x31; // 1
        case 0x13: return 0x32; // 2
        case 0x14: return 0x33; // 3
        case 0x15: return 0x34; // 4
        case 0x16: return 0x36; // 6
        case 0x17: return 0x35; // 5
        case 0x18: return 0xBB; // =
        case 0x19: return 0x39; // 9
        case 0x1A: return 0x37; // 7
        case 0x1B: return 0xBD; // -
        case 0x1C: return 0x38; // 8
        case 0x1D: return 0x30; // 0
        case 0x1E: return 0xDD; // ]
        case 0x1F: return 0x4F; // O
        case 0x20: return 0x55; // U
        case 0x21: return 0xDB; // [
        case 0x22: return 0x49; // I
        case 0x23: return 0x50; // P
        case 0x25: return 0x4C; // L
        case 0x26: return 0x4A; // J
        case 0x27: return 0xDE; // '
        case 0x28: return 0x4B; // K
        case 0x29: return 0xBA; // ;
        case 0x2A: return 0xDC; // \
        case 0x2B: return 0xBC; // ,
        case 0x2C: return 0xBF; // /
        case 0x2D: return 0x4E; // N
        case 0x2E: return 0x4D; // M
        case 0x2F: return 0xBE; // .
        case 0x31: return 0x20; // Space
        case 0x32: return 0xC0; // `
        default: return 0;
    }
}

@interface SpirationContentView : NSView <NSTextInputClient>
@property (nonatomic, assign) spiration::cocoa_window* spirationWindow;
///< 是否处于输入法组合状态（marked text）。
@property (nonatomic) BOOL imeMarked;
/// 向控件派发输入法组合文本更新（text 为空表示组合结束）。
- (void)sendImeComposition:(NSString*)text cursor:(NSInteger)cursor;
@end

@implementation SpirationContentView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actualRange {
    if (actualRange) {
        *actualRange = NSMakeRange(NSNotFound, 0);
    }
    return [[NSAttributedString alloc] initWithString:@""];
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
    spiration::cocoa_window* win = self.spirationWindow;
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
    spiration::cocoa_window* win = self.spirationWindow;

    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];

    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(self.bounds.size.height - loc.y) };
    data.button = mouse_button::left;
    data.action = mouse_action::down;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    data.consumed = false;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
    if (data.consumed) return;
}

- (void)mouseUp:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;
    win->m_IsDragging = false;
    win->m_IsResizing = false;

    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(self.bounds.size.height - loc.y) };
    data.button = mouse_button::left;
    data.action = mouse_action::up;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
}

- (void)rightMouseDown:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(self.bounds.size.height - loc.y) };
    data.button = mouse_button::right;
    data.action = mouse_action::down;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
}

- (void)rightMouseUp:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(self.bounds.size.height - loc.y) };
    data.button = mouse_button::right;
    data.action = mouse_action::up;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
}

- (void)mouseMoved:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];

    if (!win->m_IsResizing) {
#ifndef __APPLE__
        int edge = [self hitTestEdge:loc];
        [self updateResizeCursor:edge];
#endif
    }

    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(self.bounds.size.height - loc.y) };
    data.action = mouse_action::move;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
}

- (void)mouseDragged:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;

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
    data.position = { static_cast<float>(loc.x), static_cast<float>(self.bounds.size.height - loc.y) };
    data.action = mouse_action::move;
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
}

- (void)scrollWheel:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    mouse_event_data data;
    data.position = { static_cast<float>(loc.x), static_cast<float>(self.bounds.size.height - loc.y) };
    data.action = mouse_action::wheel;
    data.wheel_delta = static_cast<int>([event scrollingDeltaY] * 10);
    data.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::mouse, &data);
    }
}

- (void)keyDown:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;

    // 输入法优先：存在组合输入（如拼音）时事件由输入法消费，
    // 期间通过 setMarkedText:/insertText: 回调送达结果；
    // 未被消费的普通按键返回 NO，继续常规处理。
    if (self.inputContext && [self.inputContext handleEvent:event]) {
        return;
    }

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

    // 文本字符：macOS 方向键等功能键的 characters 是私有区字符
    // （U+F700–U+F8FF，如 ← 为 U+F702），控制字符同样不作为文本输入。
    NSString* chars = [event characters];
    if ([chars length] > 0) {
        unichar c = [chars characterAtIndex:0];
        if (c >= 0x20 && c != 0x7F && !(c >= 0xF700 && c <= 0xF8FF)) {
            key_event_data ced;
            ced.codepoint = static_cast<unsigned int>(c);
            ced.ctrl = ked.ctrl;
            ced.shift = ked.shift;
            ced.alt = ked.alt;
            if (win->get_widget()) {
                win->get_widget()->handle_event(event_type::keyboard, &ced);
            }
        }
    }
}

- (BOOL)hasMarkedText {
    return self.imeMarked;
}

- (NSRange)markedRange {
    return self.imeMarked ? NSMakeRange(0, 0) : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
    return NSMakeRange(NSNotFound, 0);
}

- (void)unmarkText {
    self.imeMarked = NO;
    [self sendImeComposition:nil cursor:-1];
}

/// 向控件派发输入法组合文本更新（text 为空表示组合结束）。
- (void)sendImeComposition:(NSString*)text cursor:(NSInteger)cursor {
    if (!self.spirationWindow || !self.spirationWindow->get_widget()) return;
    spiration::ime_composition_data data;
    if (text && [text length] > 0) {
        const char* utf8 = [text UTF8String];
        if (utf8) data.text = utf8;
        data.cursor = (cursor < 0 || cursor == NSNotFound)
            ? -1 : static_cast<int>(cursor);
    }
    self.spirationWindow->get_widget()->handle_event(event_type::ime_composition, &data);
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
    (void)replacementRange;
    NSString* text = [string isKindOfClass:[NSAttributedString class]]
        ? [(NSAttributedString*)string string]
        : (NSString*)string;
    self.imeMarked = (text && [text length] > 0);
    [self sendImeComposition:text cursor:(NSInteger)selectedRange.location];
}

- (NSArray*)validAttributesForMarkedText {
    return @[];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    if (actualRange) *actualRange = range;
    spiration::cocoa_window* win = self.spirationWindow;
    float cx = 0.0f, cy = 0.0f;
    if (win) {
        cx = win->ime_caret_x();
        cy = win->ime_caret_y();
    }
    NSRect viewRect = NSMakeRect(cx, self.bounds.size.height - cy, 1.0, 1.0);
    NSRect windowRect = [self convertRect:viewRect toView:nil];
    return [self.window convertRectToScreen:windowRect];
}

- (void)doCommandBySelector:(SEL)selector {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;
    int vk = 0;
    if (selector == @selector(insertNewline:)) vk = vk::Return;
    else if (selector == @selector(insertTab:)) vk = vk::Tab;
    else if (selector == @selector(deleteBackward:)) vk = vk::Back;
    else if (selector == @selector(deleteForward:)) vk = vk::Delete;
    else if (selector == @selector(moveLeft:)) vk = vk::Left;
    else if (selector == @selector(moveRight:)) vk = vk::Right;
    else if (selector == @selector(moveUp:)) vk = vk::Up;
    else if (selector == @selector(moveDown:)) vk = vk::Down;
    if (vk != 0 && win->get_widget()) {
        key_event_data ked;
        ked.key_code = vk;
        win->get_widget()->handle_event(event_type::keyboard, &ked);
    }
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;

    // 提交前先结束组合状态（输入法在组合结束时调用本方法）。
    self.imeMarked = NO;
    [self sendImeComposition:nil cursor:-1];

    NSString* text = [string isKindOfClass:[NSAttributedString class]]
        ? [(NSAttributedString*)string string]
        : (NSString*)string;
    if ([text length] == 0) return;

    for (NSUInteger i = 0; i < [text length]; ++i) {
        unichar c = [text characterAtIndex:i];
        unsigned int cp = static_cast<unsigned int>(c);
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < [text length]) {
            unichar low = [text characterAtIndex:i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        key_event_data ced;
        ced.key_code = 0;
        ced.codepoint = cp;
        if (win->get_widget()) {
            win->get_widget()->handle_event(event_type::keyboard, &ced);
        }
    }
}

- (void)keyUp:(NSEvent*)event {
    if (!self.spirationWindow) return;
    spiration::cocoa_window* win = self.spirationWindow;

    key_event_data ked;
    ked.key_code = cocoa_key_to_vk([event keyCode]);
    NSEventModifierFlags mods = [event modifierFlags];
    ked.ctrl = (mods & NSEventModifierFlagControl) != 0;
    ked.shift = (mods & NSEventModifierFlagShift) != 0;
    ked.alt = (mods & NSEventModifierFlagOption) != 0;

    if (win->get_widget()) {
        win->get_widget()->handle_event(event_type::keyboard, &ked);
    }
}

@end

@interface SpirationWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) spiration::cocoa_window* spirationWindow;
@end

@implementation SpirationWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
    if (self.spirationWindow) {
        self.spirationWindow->set_should_close(true);
    }
}

- (void)windowDidResize:(NSNotification*)notification {
    if (self.spirationWindow) {
        NSWindow* win = (__bridge NSWindow*)self.spirationWindow->native_handle();
        if (!win) return;

        NSView* contentView = [win contentView];
        CGSize size = contentView.bounds.size;
        CGFloat scale = [contentView.window backingScaleFactor];

        if (self.spirationWindow->get_metal_layer()) {
            CAMetalLayer* metalLayer = self.spirationWindow->get_metal_layer();
            metalLayer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
        }
        if (self.spirationWindow->gl_context()) {
            spiration_gl_update(self.spirationWindow->gl_context());
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

uint32_t cocoa_window::s_NextWindowId = 1;

cocoa_window::~cocoa_window() {
    shutdown();
}

cocoa_window::cocoa_window(cocoa_window&& other) noexcept
    : m_NSWindow(other.m_NSWindow)
    , m_NSView(other.m_NSView)
    , m_MetalLayer(other.m_MetalLayer)
    , m_GLContext(other.m_GLContext)
    , m_UseOpenGL(other.m_UseOpenGL)
    , m_Delegate(other.m_Delegate)
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
    , m_Renderer(std::move(other.m_Renderer))
    , m_Widget(std::move(other.m_Widget))
    , m_WindowId(other.m_WindowId)
{
    other.m_NSWindow = nil;
    other.m_NSView = nil;
    other.m_MetalLayer = nil;
    other.m_Delegate = nil;
    other.m_GLContext = nullptr;
    other.m_UseOpenGL = false;
    other.m_ShouldClose = false;
    other.m_IsVisible = false;
    other.m_Initialized = false;
    other.m_WindowId = 0;
    other.m_BackingScale = 2.0f;
}

cocoa_window& cocoa_window::operator=(cocoa_window&& other) noexcept {
    if (this != &other) {
        shutdown();

        m_NSWindow = other.m_NSWindow;
        m_NSView = other.m_NSView;
        m_MetalLayer = other.m_MetalLayer;
        m_Delegate = other.m_Delegate;
        m_GLContext = other.m_GLContext;
        m_UseOpenGL = other.m_UseOpenGL;
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
        m_Renderer = std::move(other.m_Renderer);
        m_Widget = std::move(other.m_Widget);
        m_WindowId = other.m_WindowId;

        other.m_NSWindow = nil;
        other.m_NSView = nil;
        other.m_MetalLayer = nil;
        other.m_Delegate = nil;
        other.m_GLContext = nullptr;
        other.m_UseOpenGL = false;
        other.m_ShouldClose = false;
        other.m_IsVisible = false;
        other.m_Initialized = false;
        other.m_WindowId = 0;
        other.m_BackingScale = 2.0f;
    }
    return *this;
}

void cocoa_window::show() {
    @autoreleasepool {
        [m_NSWindow makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        m_IsVisible = true;
    }
}

void cocoa_window::hide() {
    @autoreleasepool {
        [m_NSWindow orderOut:nil];
        m_IsVisible = NO;
    }
}

void cocoa_window::maximize() {
    @autoreleasepool {
        [m_NSWindow zoom:nil];
        m_IsMaximized = YES;
    }
}

void cocoa_window::minimize() {
    @autoreleasepool {
        [m_NSWindow miniaturize:nil];
        m_IsMinimized = YES;
    }
}

void cocoa_window::restore() {
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

void cocoa_window::close() {
    if (m_ShouldClose) return;
    m_ShouldClose = YES;
}

std::string cocoa_window::title() const {
    return m_Title;
}

void cocoa_window::set_title(const std::string& title) {
    m_Title = title;
    @autoreleasepool {
        [m_NSWindow setTitle:[NSString stringWithUTF8String:title.c_str()]];
    }
}

void cocoa_window::get_size(int32_t& width, int32_t& height) const {
    width = m_Width;
    height = m_Height;
}

void cocoa_window::set_size(int32_t width, int32_t height) {
    m_Width = width;
    m_Height = height;
    @autoreleasepool {
        NSRect frame = [m_NSWindow frame];
        frame.size.width = static_cast<CGFloat>(width);
        frame.size.height = static_cast<CGFloat>(height);
        [m_NSWindow setFrame:frame display:YES animate:NO];

        if (m_MetalLayer) {
            CAMetalLayer* layer = m_MetalLayer;
            CGFloat scale = [m_NSWindow backingScaleFactor];
            layer.drawableSize = CGSizeMake(width * scale, height * scale);
            m_BackingScale = static_cast<float>(scale);
        }
        if (m_GLContext) {
            spiration_gl_update(m_GLContext);
        }
    }
    if (m_Renderer) {
        m_Renderer->resize(static_cast<uint32_t>(width * m_BackingScale),
                           static_cast<uint32_t>(height * m_BackingScale));
    }
    notify_widget_resize();
}

void cocoa_window::get_position(int32_t& x, int32_t& y) const {
    x = m_X;
    y = m_Y;
}

void cocoa_window::set_position(int32_t x, int32_t y) {
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

bool cocoa_window::is_visible() const { return m_IsVisible; }
bool cocoa_window::is_maximized() const {
    return [m_NSWindow isZoomed];
}
bool cocoa_window::is_minimized() const { return m_IsMinimized; }
bool cocoa_window::is_fullscreen() const { return m_IsFullscreen; }

void cocoa_window::set_fullscreen(bool fullscreen) {
    @autoreleasepool {
        if (fullscreen != m_IsFullscreen) {
            [m_NSWindow toggleFullScreen:nil];
            m_IsFullscreen = fullscreen;
        }
    }
}

void* cocoa_window::native_handle() const {
    return (__bridge void*)m_NSWindow;
}

void cocoa_window::loop() {
    @autoreleasepool {
        if (m_ShouldClose) return;

        static NSTimeInterval lastTick = [NSDate timeIntervalSinceReferenceDate];
        NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
        float dt_ms = static_cast<float>((now - lastTick) * 1000.0);
        lastTick = now;
        if (dt_ms > 100.0f) dt_ms = 16.0f;

        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate dateWithTimeIntervalSinceNow:0.016]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }

        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.0]];

        if (!m_ShouldClose && m_Widget && m_Renderer) {
            if (m_NeedsLayout) {
                m_Widget->layout();
                m_NeedsLayout = false;
            }
            m_Widget->tick(dt_ms);
            m_Renderer->begin_frame();
            m_Widget->paint(m_Renderer);
            m_Renderer->end_frame();
        }
    }
}

bool cocoa_window::should_close() const {
    return m_ShouldClose;
}

void cocoa_window::request_repaint() {
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

void cocoa_window::request_layout() {
    m_NeedsLayout = true;
}

void cocoa_window::set_cursor(cursor_style c) {
    @autoreleasepool {
        NSCursor* cur = [NSCursor arrowCursor];
        switch (c) {
        case cursor_style::text:        cur = [NSCursor IBeamCursor]; break;
        case cursor_style::pointer:     cur = [NSCursor pointingHandCursor]; break;
        case cursor_style::crosshair:   cur = [NSCursor crosshairCursor]; break;
        case cursor_style::move:        cur = [NSCursor openHandCursor]; break;
        case cursor_style::resize_h:    cur = [NSCursor resizeLeftRightCursor]; break;
        case cursor_style::resize_v:    cur = [NSCursor resizeUpDownCursor]; break;
        case cursor_style::resize_nwse: cur = [NSCursor closedHandCursor]; break;
        case cursor_style::resize_nesw: cur = [NSCursor closedHandCursor]; break;
        case cursor_style::forbidden:   cur = [NSCursor operationNotAllowedCursor]; break;
        case cursor_style::default_cursor:
        default:                       break;
        }
        [cur set];
    }
}

void cocoa_window::set_mouse_capture(bool capture) {
    if (capture) {
        [NSCursor hide];
        CGAssociateMouseAndMouseCursorPosition(false);
    } else {
        CGAssociateMouseAndMouseCursorPosition(true);
        [NSCursor unhide];
    }
}

void cocoa_window::set_widget(std::unique_ptr<widget> widget) {
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

bool cocoa_window::initialize(const window_parameters& params) {
    m_WindowId = s_NextWindowId++;
    m_Title = params.title;
    m_Width = params.width;
    m_Height = params.height;

    @autoreleasepool {
        if (!create_cocoa_window(params)) return false;
    }

    if (!create_renderer()) return false;

    ime::set_caret_callback([this](float x, float y) {
        m_ImeCaretX = x;
        m_ImeCaretY = y;
    });

    m_Initialized = true;

    if (params.visible) {
        show();
    }

    return true;
}

void cocoa_window::shutdown() {
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
        m_Delegate = nil;
    }

    if (m_GLContext) {
        spiration_gl_destroy(m_GLContext);
        m_GLContext = nullptr;
    }

    m_Initialized = false;
}

bool cocoa_window::create_cocoa_window(const window_parameters& params) {
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
        m_Delegate = delegate;  // NSWindow 的 delegate 是弱引用，必须强持有
        [m_NSWindow setDelegate:delegate];

        NSRect viewRect = NSMakeRect(0, 0, params.width, params.height);
        SpirationContentView* contentView = [[SpirationContentView alloc] initWithFrame:viewRect];
        contentView.spirationWindow = this;
        m_NSView = contentView;

        id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
        if (metalDevice) {
            contentView.wantsLayer = YES;
            m_MetalLayer = [CAMetalLayer layer];
            m_MetalLayer.device = metalDevice;
            m_MetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            m_MetalLayer.framebufferOnly = YES;
            m_MetalLayer.frame = viewRect;

            CGFloat scale = [m_NSWindow backingScaleFactor];
            m_MetalLayer.drawableSize = CGSizeMake(params.width * scale,
                                                    params.height * scale);
            m_BackingScale = static_cast<float>(scale);

            [m_NSView setLayer:m_MetalLayer];
        } else {
            m_UseOpenGL = true;
        }

        [m_NSWindow setContentView:m_NSView];
        [m_NSWindow makeFirstResponder:m_NSView];

        if (m_UseOpenGL) {
            m_GLContext = spiration_gl_create_context((__bridge void*)m_NSView);
            if (!m_GLContext) {
                NSLog(@"Failed to create OpenGL context");
                return false;
            }
            m_BackingScale = static_cast<float>([m_NSWindow backingScaleFactor]);
        }

        [m_NSWindow center];

        NSRect frame = [m_NSWindow frame];
        m_X = static_cast<int32_t>(frame.origin.x);
        m_Y = static_cast<int32_t>(frame.origin.y);
        NSRect content = [m_NSWindow contentRectForFrameRect:frame];
        m_Width = static_cast<int32_t>(content.size.width);
        m_Height = static_cast<int32_t>(content.size.height);

        return true;
    }
}

bool cocoa_window::create_renderer() {
    if (m_Renderer) return true;

    if (!m_UseOpenGL) {
        m_Renderer = renderer::create_metal_renderer();
        if (m_Renderer && m_Renderer->initialize((__bridge void*)m_MetalLayer)) {
            CGFloat scale = [m_NSWindow backingScaleFactor];
            m_Renderer->resize(static_cast<uint32_t>(m_Width * scale),
                               static_cast<uint32_t>(m_Height * scale));
            return true;
        }
        m_Renderer.reset();
        NSLog(@"Metal renderer unavailable, falling back to OpenGL");
    }

    if (!m_GLContext) {
        m_GLContext = spiration_gl_create_context((__bridge void*)m_NSView);
        if (!m_GLContext) {
            NSLog(@"Failed to create OpenGL context");
            return false;
        }
    }
    m_Renderer = renderer::create_opengl_renderer();
    if (m_Renderer && m_Renderer->initialize(m_GLContext)) {
        CGFloat scale = [m_NSWindow backingScaleFactor];
        m_Renderer->resize(static_cast<uint32_t>(m_Width * scale),
                           static_cast<uint32_t>(m_Height * scale));
        return true;
    }
    m_Renderer.reset();
    return false;
}

void cocoa_window::notify_widget_resize() {
    if (!m_Widget) return;
    size sizeData = { static_cast<float>(m_Width), static_cast<float>(m_Height) };
    m_Widget->handle_event(event_type::window_resize, &sizeData);
}

std::shared_ptr<window> window::create() {
    return create(window_parameters{});
}

std::shared_ptr<window> window::create(const window_parameters& params) {
    auto win = std::make_shared<cocoa_window>();
    if (!win->initialize(params)) return nullptr;
    return win;
}

window::window() = default;
window::~window() = default;
window::window(window&&) noexcept = default;
window& window::operator=(window&&) noexcept = default;

} // namespace spiration
