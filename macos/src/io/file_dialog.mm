/**
 * @file file_dialog.mm
 * @brief 文件对话框实现。
 * @author clk
 */

#include <io/file_dialog.h>
#include <string>
#include <vector>

#import <AppKit/AppKit.h>

namespace spiration {
namespace io {

static NSString* build_filter(const std::string& desc,
                               const std::vector<std::string>& patterns) {
    if (!patterns.empty() && patterns[0] != "*") {
        std::string ext = patterns[0];
        if (ext.size() > 2 && ext.substr(0, 2) == "*.")
            ext = ext.substr(2);
        return [NSString stringWithUTF8String:ext.c_str()];
    }
    return nil;
}

std::string open_file(const std::string& title,
                      const std::string& filter,
                      const std::vector<std::string>& patterns) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.title = [NSString stringWithUTF8String:title.c_str()];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;

        if (!patterns.empty() && patterns[0] != "*") {
            NSMutableArray* exts = [NSMutableArray array];
            for (const auto& p : patterns) {
                std::string ext = p;
                if (ext.size() > 2 && ext.substr(0, 2) == "*.")
                    ext = ext.substr(2);
                [exts addObject:[NSString stringWithUTF8String:ext.c_str()]];
            }
            panel.allowedFileTypes = exts;
        }

        if ([panel runModal] == NSModalResponseOK) {
            const char* utf8Path = [[panel.URL path] UTF8String];
            return utf8Path ? std::string(utf8Path) : std::string();
        }
        return {};
    }
}

std::string save_file(const std::string& title,
                      const std::string& filter,
                      const std::vector<std::string>& patterns) {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.title = [NSString stringWithUTF8String:title.c_str()];

        if (!patterns.empty() && patterns[0] != "*") {
            std::string ext = patterns[0];
            if (ext.size() > 2 && ext.substr(0, 2) == "*.")
                ext = ext.substr(2);
            panel.allowedFileTypes = @[[NSString stringWithUTF8String:ext.c_str()]];
        }

        if ([panel runModal] == NSModalResponseOK) {
            const char* utf8Path = [[panel.URL path] UTF8String];
            return utf8Path ? std::string(utf8Path) : std::string();
        }
        return {};
    }
}

} // namespace io
} // namespace spiration
