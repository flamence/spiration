/**
 * @file ohos_clipboard.cpp
 * @brief HarmonyOS 剪贴板实现
 * @author clk
 */

#include "ohos_clipboard.h"
#include <utils/clipboard.h>
#include <utils/console.h>
#include <napi/native_api.h>
#include <string>

namespace spiration {

// NAPI 环境指针（由 napi_bridge 初始化）
static napi_env g_napi_env = nullptr;

// 设置 NAPI 环境
void set_clipboard_napi_env(napi_env env) {
    g_napi_env = env;
}

// 辅助函数：调用 ArkTS 剪贴板函数
static std::string call_arkts_clipboard_function(const char* function_name, const std::string& arg = "") {
    if (!g_napi_env) {
        console::error("clipboard", "NAPI environment not initialized");
        return "";
    }

    napi_value global;
    napi_get_global(g_napi_env, &global);

    // 获取 nativeEntry 对象
    napi_value native_entry;
    napi_value native_entry_str;
    napi_create_string_utf8(g_napi_env, "nativeEntry", NAPI_AUTO_LENGTH, &native_entry_str);
    napi_get_property(g_napi_env, global, native_entry_str, &native_entry);

    // 获取剪贴板函数
    napi_value func;
    napi_value func_name;
    napi_create_string_utf8(g_napi_env, function_name, NAPI_AUTO_LENGTH, &func_name);
    napi_get_property(g_napi_env, native_entry, func_name, &func);

    // 调用函数
    napi_value result;
    if (arg.empty()) {
        napi_call_function(g_napi_env, native_entry, func, 0, nullptr, &result);
    } else {
        napi_value arg_value;
        napi_create_string_utf8(g_napi_env, arg.c_str(), NAPI_AUTO_LENGTH, &arg_value);
        napi_call_function(g_napi_env, native_entry, func, 1, &arg_value, &result);
    }

    // 获取返回值
    napi_valuetype result_type;
    napi_typeof(g_napi_env, result, &result_type);
    
    if (result_type == napi_string) {
        size_t length = 0;
        napi_get_value_string_utf8(g_napi_env, result, nullptr, 0, &length);
        if (length > 0) {
            char* buffer = new char[length + 1];
            napi_get_value_string_utf8(g_napi_env, result, buffer, length + 1, &length);
            std::string text(buffer);
            delete[] buffer;
            return text;
        }
    }

    return "";
}

// 复制文本到剪贴板
void clipboard::copy(const std::string& text) {
    if (text.empty()) {
        console::warning("clipboard", "Attempted to copy empty text");
        return;
    }

    try {
        call_arkts_clipboard_function("copyToClipboard", text);
        console::info("clipboard", "Text copied to clipboard");
    } catch (const std::exception& e) {
        console::error("clipboard", "Failed to copy text: %s", e.what());
    }
}

// 从剪贴板粘贴文本
std::string clipboard::paste() {
    try {
        std::string text = call_arkts_clipboard_function("pasteFromClipboard");
        if (!text.empty()) {
            console::info("clipboard", "Text pasted from clipboard");
        }
        return text;
    } catch (const std::exception& e) {
        console::error("clipboard", "Failed to paste text: %s", e.what());
        return "";
    }
}

} // namespace spiration
