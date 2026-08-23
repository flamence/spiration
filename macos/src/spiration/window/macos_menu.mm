/**
 * @file macos_menu.mm
 * @author clk
 */

#import <Cocoa/Cocoa.h>

#include <functional>
#include <string>

#include <spiration/window/macos_menu.h>

namespace {

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

void* spiration_macos_menu_get_submenu(const char* key, const char* title) {
    if (!key || !title) return nullptr;

    NSMenu* main_menu = [NSApp mainMenu];
    if (!main_menu) {
        NSLog(@"[macos_menu] mainMenu is NIL");
        return nullptr;
    }
    NSLog(@"[macos_menu] get_submenu key='%s' title='%s' mainMenu items=%lu",
          key, title, (unsigned long)[[NSApp mainMenu] numberOfItems]);

    NSString* ns_key = [NSString stringWithUTF8String:key];
    static NSMutableDictionary* submenus = nil;
    if (!submenus) {
        submenus = [NSMutableDictionary dictionary];
    }

    NSMenu* menu = [submenus objectForKey:ns_key];
    if (!menu) {
        NSString* ns_title = [NSString stringWithUTF8String:title];
        menu = [[NSMenu alloc] initWithTitle:ns_title];
        NSMenuItem* top = [[NSMenuItem alloc] initWithTitle:ns_title
                                                     action:nil
                                              keyEquivalent:@""];
        [top setSubmenu:menu];
        [main_menu addItem:top];
        [submenus setObject:menu forKey:ns_key];
    }
    return (__bridge void*)menu;
}

int spiration_macos_menu_add_item(void* submenu, const char* title, void* callback) {
    NSMenu* menu = (__bridge NSMenu*)submenu;
    if (!menu || !title || !callback) return 0;
    NSLog(@"[macos_menu] add_item title='%s'", title);

    SpirationMenuTarget* target =
        [[SpirationMenuTarget alloc] initWithCallback:(std::function<void()>*)callback];
    if (!menu_targets) {
        menu_targets = [NSMutableArray array];
    }
    [menu_targets addObject:target];

    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:title]
                                                  action:@selector(invoke:)
                                           keyEquivalent:@""];
    [item setTarget:target];
    [menu addItem:item];
    return 1;
}

} // extern "C"
