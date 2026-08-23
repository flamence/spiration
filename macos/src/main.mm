/**
 * @file main.mm
 * @brief 应用入口。
 * @author clk
 */

#include <spiration/application.h>

#import <Cocoa/Cocoa.h>

@interface SpirationAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation SpirationAppDelegate

- (BOOL)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    spiration::application::instance()->shutdown();
    return YES;
}

@end

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
    [NSApp activateIgnoringOtherApps:YES];
    instance->loop();
    instance->shutdown();

    return 0;
}
