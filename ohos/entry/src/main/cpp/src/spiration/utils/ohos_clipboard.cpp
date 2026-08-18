/**
 * @file ohos_clipboard.cpp
 * @brief HarmonyOS 剪贴板实现
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/utils/ohos_clipboard.h>
#include <spiration/utils/clipboard.h>
#include <string>

namespace spiration {

// 复制文本到剪贴板
void clipboard::copy(const std::string& text) {
    if (text.empty()) {
        application::instance()->logger()->log({logging::level::warning, "Attempted to copy empty text", "clipboard"});
        return;
    }

    if (clipboard_copy_to_arkts(text)) {
        application::instance()->logger()->log({logging::level::info, "Text copied to clipboard", "clipboard"});
    } else {
        application::instance()->logger()->log({logging::level::warning, "clipboard callback not registered", "clipboard"});
    }
}

// 从剪贴板粘贴文本
std::string clipboard::paste() {
    std::string text = clipboard_paste_from_arkts();
    if (!text.empty()) {
        application::instance()->logger()->log({logging::level::info, "Text pasted from clipboard", "clipboard"});
    }
    return text;
}

} // namespace spiration
