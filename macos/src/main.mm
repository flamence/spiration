/**
 * @file main.mm
 * @brief 应用入口。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/ui/about_tab.h>
#include <spiration/ui/extension_tab.h>
#include <spiration/ui/root.h>
#include <spiration/window/macos_menu.h>

#include <functional>

#import <Cocoa/Cocoa.h>

@interface SpirationAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation SpirationAppDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    spiration::application::instance()->shutdown();
    return NSTerminateNow;
}

@end

/// 构建原生核心菜单：自绘 appbar 在 macOS 已禁用，
/// 其提供的 文件>退出、帮助>关于/拓展 由原生菜单栏承担。
/// 须在 application::initialize() 之后调用（此时 i18n 已加载、扩展条目已就位）。
static void build_core_menus() {
    auto& i18n = spiration::i18n_manager::get();

    void* file_menu = spiration_macos_menu_get_submenu("menu.file", i18n.tr("menu.file").c_str());
    if (file_menu) {
        auto* quit_cb = new std::function<void()>([]() { [NSApp terminate:nil]; });
        spiration_macos_menu_add_item(file_menu, i18n.tr("menu.file.exit").c_str(), quit_cb);
    }

    void* help_menu = spiration_macos_menu_get_submenu("menu.help", i18n.tr("menu.help").c_str());
    if (help_menu) {
        auto* about_cb = new std::function<void()>([]() {
            if (auto* r = dynamic_cast<spiration::root*>(spiration::application::instance()->widget())) {
                r->open_tab(std::make_unique<spiration::about_tab>());
            }
        });
        spiration_macos_menu_add_item(help_menu, i18n.tr("menu.help.about").c_str(), about_cb);

        auto* ext_cb = new std::function<void()>([]() {
            if (auto* r = dynamic_cast<spiration::root*>(spiration::application::instance()->widget())) {
                r->open_tab(std::make_unique<spiration::extension_tab>());
            }
        });
        spiration_macos_menu_add_item(help_menu, i18n.tr("menu.help.extensions").c_str(), ext_cb);
    }
}

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;

    static SpirationAppDelegate* appDelegate = [[SpirationAppDelegate alloc] init];

    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app setDelegate:appDelegate];

        NSMenu* menubar = [[NSMenu alloc] init];
        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        [app setMainMenu:menubar];

        NSMenu* appMenu = [[NSMenu alloc] init];
        NSString* appName = @"Spiration";
        NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit Spiration"
                                                          action:@selector(terminate:)
                                                   keyEquivalent:@"q"];
        [appMenu addItem:quitItem];
        [appMenuItem setSubmenu:appMenu];
    }

    auto instance = spiration::application::instance();
    if (!instance->initialize()) {
        return 1;
    }
    build_core_menus();
    [NSApp activateIgnoringOtherApps:YES];
    instance->loop();
    instance->shutdown();

    return 0;
}
