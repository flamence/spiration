/**
 * @file clipboard.mm
 * @brief 剪贴板实现。
 * @author clk
 */

#include <spiration/utils/clipboard.h>
#include <string>

#import <Cocoa/Cocoa.h>

namespace spiration {

void clipboard::copy(const std::string& text) {
    if (text.empty()) return;
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        NSString *string = [NSString stringWithUTF8String:text.c_str()];
        [pasteboard setString:string forType:NSPasteboardTypeString];
    }
}

std::string clipboard::paste() {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSString *string = [pasteboard stringForType:NSPasteboardTypeString];
        if (string) {
            return std::string([string UTF8String]);
        }
    }
    return {};
}

} // namespace spiration
