/**
 * @file main.mm
 * @brief 应用入口。
 * @author clk
 */

#import <application.h>
#import <utils/console.h>
#import <utils/crash_log.h>
#import <utils/platform.h>
#import <Cocoa/Cocoa.h>

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;
    std::string log_dir = spiration::platform::join_path(
        spiration::platform::executable_directory(), "logs");
    spiration::platform::create_directory(log_dir);
    std::string log_path = spiration::console::make_log_path(log_dir, "spiration");
    spiration::console::set_log_file(log_path);
    spiration::crash_log::install(log_path);

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
    instance->initialize();
    [NSApp activateIgnoringOtherApps:YES];
    instance->loop();
    instance->shutdown();

    return 0;
}
