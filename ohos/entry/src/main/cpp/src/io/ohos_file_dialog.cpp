/**
 * @file file_dialog.cpp
 * @brief 文件选择对话框实现。
 * @author clk
 */

#include <io/file_dialog.h>
#include <io/ohos_file_dialog.h>
#include "napi/native_api.h"
#include <utils/console.h>

#include <functional>

static napi_env g_pickerEnv = nullptr;
static napi_ref g_pickerRef = nullptr;

napi_value NapiRegisterFilePickerCallback(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    g_pickerEnv = env;
    if (g_pickerRef) {
        napi_delete_reference(env, g_pickerRef);
        g_pickerRef = nullptr;
    }
    napi_create_reference(env, args[0], 1, &g_pickerRef);
    return args[0];
}

namespace spiration {
namespace io {

namespace {
std::function<void(const std::string&, const std::string&)> g_open_cb;  // (uri, content)
std::function<void(const std::string&)> g_save_cb;                      // (uri)
} // namespace

std::string open_file(const std::string& title, const std::string& filter, const std::vector<std::string>& patterns) {
    console::warning("io", "sync open_file unsupported on OHOS, use open_file_async");
    (void)title;
    (void)filter;
    (void)patterns;
    return "";
}

std::string save_file(const std::string& title, const std::string& filter, const std::vector<std::string>& patterns) {
    console::warning("io", "sync save_file unsupported on OHOS, use save_file_async");
    (void)title;
    (void)filter;
    (void)patterns;
    return "";
}

void open_file_async(const std::string& title, const std::string& filter,
                     const std::vector<std::string>& patterns,
                     std::function<void(const std::string&, const std::string&)> on_result) {
    (void)filter;
    (void)patterns;
    g_open_cb = std::move(on_result);
    g_save_cb = nullptr;
    if (!invoke_file_picker(title, false, "")) {
        console::error("io", "file picker callback not registered");
        if (g_open_cb) {
            auto cb = std::move(g_open_cb);
            g_open_cb = nullptr;
            cb("", "");
        }
    }
}

void save_file_async(const std::string& title, const std::string& filter,
                     const std::vector<std::string>& patterns,
                     const std::string& content,
                     std::function<void(const std::string&)> on_result) {
    (void)filter;
    (void)patterns;
    g_save_cb = std::move(on_result);
    g_open_cb = nullptr;
    if (!invoke_file_picker(title, true, content)) {
        console::error("io", "file picker callback not registered");
        if (g_save_cb) {
            auto cb = std::move(g_save_cb);
            g_save_cb = nullptr;
            cb("");
        }
    }
}

void deliver_file_dialog_result(const std::string& uri, const std::string& content) {
    if (g_open_cb) {
        auto cb = std::move(g_open_cb);
        g_open_cb = nullptr;
        cb(uri, content);
    } else if (g_save_cb) {
        auto cb = std::move(g_save_cb);
        g_save_cb = nullptr;
        cb(uri);
    }
}

bool invoke_file_picker(const std::string& title, bool is_save,
                        const std::string& content) {
    if (!g_pickerEnv || !g_pickerRef) return false;
    napi_value cb;
    napi_get_reference_value(g_pickerEnv, g_pickerRef, &cb);
    napi_value undefined;
    napi_get_undefined(g_pickerEnv, &undefined);
    napi_value titleVal;
    napi_create_string_utf8(g_pickerEnv, title.c_str(), title.size(), &titleVal);
    napi_value saveVal;
    napi_get_boolean(g_pickerEnv, is_save, &saveVal);
    napi_value contentVal;
    napi_create_string_utf8(g_pickerEnv, content.c_str(), content.size(), &contentVal);
    napi_value argv[3] = {titleVal, saveVal, contentVal};
    napi_call_function(g_pickerEnv, undefined, cb, 3, argv, nullptr);
    return true;
}

} // namespace io
} // namespace spiration