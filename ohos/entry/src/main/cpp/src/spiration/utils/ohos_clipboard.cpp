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
        application::instance()->logger()->log({ .level = logging::level::warning, .message = "Attempted to copy empty text", .tag = "clipboard" });
        return;
    }

    if (clipboard_copy_to_arkts(text)) {
        application::instance()->logger()->log({ .level = logging::level::info, .message = "Text copied to clipboard", .tag = "clipboard" });
    } else {
        application::instance()->logger()->log({ .level = logging::level::warning, .message = "clipboard callback not registered", .tag = "clipboard" });
    }
}

// 从剪贴板粘贴文本
std::string clipboard::paste() {
    std::string text = clipboard_paste_from_arkts();
    if (!text.empty()) {
        application::instance()->logger()->log({ .level = logging::level::info, .message = "Text pasted from clipboard", .tag = "clipboard" });
    }
    return text;
}

} // namespace spiration
