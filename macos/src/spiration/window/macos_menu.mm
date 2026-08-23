/**
 * @file macos_menu.mm
 * @author clk
 * @brief macOS 原生菜单栏桥接：将应用与扩展注册的菜单挂到系统菜单栏。
 */

#import <Cocoa/Cocoa.h>

#include <functional>
#include <string>

#include <spiration/window/macos_menu.h>
#include <spiration/window/macos_menu_state.h>

namespace {

/// NSMenuItem 的 target 为弱引用，这里强持有全部回调 target。
NSMutableArray* menu_targets = nil;

} // namespace

@interface SpirationMenuTarget : NSObject {
    std::function<void()>* callback_;
}
- (instancetype)initWithCallback:(std::function<void()>*)callback;
- (void)invoke:(id)sender;
@end

@implementation SpirationMenuTarget

- (instancetype)initWithCallback:(std::function<void()>*)callback {
    if ((self = [super init])) {
        callback_ = callback;
    }
    return self;
}

- (void)invoke:(id)sender {
    (void)sender;
    if (callback_ && *callback_) {
        (*callback_)();
    }
}

@end

extern "C" {

void* spiration_macos_menu_get_submenu(const spiration_macos_menu_spec* spec) {
    if (!spec || !spec->key) return nullptr;
    NSLog(@"[macos_menu] get_submenu spec=%p key='%s'(%p) title=%p", spec, spec->key, spec->key, spec->title);

    NSMenu* main_menu = [NSApp mainMenu];
    if (!main_menu) return nullptr;

    NSString* ns_key = [NSString stringWithUTF8String:spec->key];
    NSString* ns_title = (spec->title && spec->title[0])
        ? [NSString stringWithUTF8String:spec->title] : ns_key;

    static NSMutableDictionary* submenus = nil;
    if (!submenus) {
        submenus = [NSMutableDictionary dictionary];
    }

    NSMenu* menu = [submenus objectForKey:ns_key];
    if (!menu) {
        menu = [[NSMenu alloc] initWithTitle:ns_title];
        NSMenuItem* top = [[NSMenuItem alloc] initWithTitle:ns_title
                                                     action:nil
                                              keyEquivalent:@""];
        [top setSubmenu:menu];
        [main_menu addItem:top];
        [submenus setObject:menu forKey:ns_key];
        NSLog(@"[macos_menu] created top-level '%@' (key=%s)", ns_title, spec->key);
    }
    return (__bridge void*)menu;
}

int spiration_macos_menu_add_item(const spiration_macos_menu_spec* spec) {
    NSLog(@"[macos_menu] add_item spec=%p submenu=%p state=%p",
          spec, spec ? spec->submenu : nullptr, spec ? spec->callback : nullptr);
    NSMenu* menu = (__bridge NSMenu*)(spec ? spec->submenu : nullptr);
    if (!menu || !spec || !spec->callback) return 0;

    // 标题与回调同处堆上状态对象，经堆内字符串读取。
    auto* state = static_cast<spiration_menu_item_state*>(spec->callback);
    const char* title_text = state->title.c_str();
    const unsigned char* tp = (const unsigned char*)title_text;

    // 显式 UTF-8 构造 NSString；失败（非法 UTF-8）则拒绝添加，
    // 避免旧式 std::function* 回调被误读为状态对象时崩溃。
    NSString* ns_title = [NSString stringWithUTF8String:title_text];
    if (!ns_title) {
        NSLog(@"[macos_menu] add item FAILED: title not valid UTF-8 "
              @"(title ptr=%p bytes: %02x %02x %02x %02x %02x %02x %02x %02x)",
              title_text, tp[0], tp[1], tp[2], tp[3], tp[4], tp[5], tp[6], tp[7]);
        return 0;
    }
    // 注意：菜单项标题以 %@ 打印（实际进入 NSMenuItem 的字符串）。
    // %s 在部分系统编码下无法显示 UTF-8 中文，会误显示为空。
    NSLog(@"[macos_menu] add item title='%@' (state=%p title ptr=%p "
          @"bytes: %02x %02x %02x %02x %02x %02x %02x %02x)",
          ns_title, state, title_text,
          tp[0], tp[1], tp[2], tp[3], tp[4], tp[5], tp[6], tp[7]);

    SpirationMenuTarget* target =
        [[SpirationMenuTarget alloc] initWithCallback:&state->callback];
    if (!menu_targets) {
        menu_targets = [NSMutableArray array];
    }
    [menu_targets addObject:target];

    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:ns_title
                                                  action:@selector(invoke:)
                                           keyEquivalent:@""];
    [item setTarget:target];
    [menu addItem:item];
    return 1;
}

} // extern "C"
