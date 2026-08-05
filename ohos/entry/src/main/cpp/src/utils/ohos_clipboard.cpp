/**
 * @file ohos_clipboard.cpp
 * @brief HarmonyOS 剪贴板实现
 * @author clk
 */

#include <utils/ohos_clipboard.h>
#include <utils/clipboard.h>
#include <utils/console.h>
#include <string>

namespace spiration {

// 复制文本到剪贴板
void clipboard::copy(const std::string& text) {
    if (text.empty()) {
        console::warning("clipboard", "Attempted to copy empty text");
        return;
    }

    if (clipboard_copy_to_arkts(text)) {
        console::info("clipboard", "Text copied to clipboard");
    } else {
        console::warning("clipboard", "clipboard callback not registered");
    }
}

// 从剪贴板粘贴文本
std::string clipboard::paste() {
    std::string text = clipboard_paste_from_arkts();
    if (!text.empty()) {
        console::info("clipboard", "Text pasted from clipboard");
    }
    return text;
}

} // namespace spiration
