/**
 * @file main.mm
 * @brief 应用入口。
 * @author clk
 */

#include <spiration/application.h>

#import <Cocoa/Cocoa.h>

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;

    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

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
        // 窗口/渲染器创建失败（例如设备不支持 Metal 或 shader 库无法加载），
        // 此时 window_ 为空，直接进入 loop() 会空指针崩溃。
        NSLog(@"Spiration failed to initialize");
        @autoreleasepool {
            NSAlert* alert = [[NSAlert alloc] init];
            alert.messageText = @"Spiration 启动失败";
            alert.informativeText = @"无法创建应用窗口（Metal 与 OpenGL 均不可用）。虚拟机用户请检查是否已开启 3D 加速。";
            [alert runModal];
        }
        return 1;
    }
    [NSApp activateIgnoringOtherApps:YES];
    instance->loop();
    instance->shutdown();

    return 0;
}
