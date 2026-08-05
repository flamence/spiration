/**
 * @file ohos_file_dialog.h
 * @brief 文件选择对话框实现。
 * @author clk
 */

#pragma once

#include <string>

#include "napi/native_api.h"

napi_value NapiRegisterFilePickerCallback(napi_env env, napi_callback_info info);

namespace spiration {
namespace io {

bool invoke_file_picker(const std::string& title, bool is_save,
                        const std::string& content);

} // namespace io
} // namespace spiration