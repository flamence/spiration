/**
 * @file macos_menu.mm
 * @author clk
 * @brief macOS 原生菜单栏桥接：将应用与扩展注册的菜单挂到系统菜单栏。
 */

#import <Cocoa/Cocoa.h>

#include <functional>
#include <string>

#include <spiration/window/macos_menu.h>

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
    NSMenu* menu = (__bridge NSMenu*)(spec ? spec->submenu : nullptr);
    if (!menu || !spec || !spec->callback) return 0;

    // 标题为空时回退键名（此处键名不可用，回退为空字符串而非崩溃）。
    const char* title_text = (spec->title && spec->title[0]) ? spec->title : "";
    NSLog(@"[macos_menu] add item '%s'", title_text);

    SpirationMenuTarget* target =
        [[SpirationMenuTarget alloc] initWithCallback:(std::function<void()>*)spec->callback];
    if (!menu_targets) {
        menu_targets = [NSMutableArray array];
    }
    [menu_targets addObject:target];

    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:title_text]
                                                  action:@selector(invoke:)
                                           keyEquivalent:@""];
    [item setTarget:target];
    [menu addItem:item];
    return 1;
}

} // extern "C"
