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

    spiration_macos_menu_spec file_spec{};
    file_spec.key = "menu.file";
    std::string file_title = i18n.tr("menu.file");
    file_spec.title = file_title.c_str();
    void* file_menu = spiration_macos_menu_get_submenu(&file_spec);
    if (file_menu) {
        auto* quit_cb = new std::function<void()>([]() { [NSApp terminate:nil]; });
        spiration_macos_menu_spec item_spec{};
        item_spec.submenu = file_menu;
        std::string exit_title = i18n.tr("menu.file.exit");
        item_spec.title = exit_title.c_str();
        item_spec.callback = quit_cb;
        spiration_macos_menu_add_item(&item_spec);
    }

    spiration_macos_menu_spec help_spec{};
    help_spec.key = "menu.help";
    std::string help_title = i18n.tr("menu.help");
    help_spec.title = help_title.c_str();
    void* help_menu = spiration_macos_menu_get_submenu(&help_spec);
    if (help_menu) {
        auto add_core_item = [&](const std::string& key, std::function<void()> cb) {
            auto* box = new std::function<void()>(std::move(cb));
            spiration_macos_menu_spec item_spec{};
            item_spec.submenu = help_menu;
            std::string item_title = i18n.tr(key);
            item_spec.title = item_title.c_str();
            item_spec.callback = box;
            spiration_macos_menu_add_item(&item_spec);
        };
        add_core_item("menu.help.about", []() {
            if (auto* r = dynamic_cast<spiration::root*>(spiration::application::instance()->widget())) {
                r->open_tab(std::make_unique<spiration::about_tab>());
            }
        });
        add_core_item("menu.help.extensions", []() {
            if (auto* r = dynamic_cast<spiration::root*>(spiration::application::instance()->widget())) {
                r->open_tab(std::make_unique<spiration::extension_tab>());
            }
        });
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
