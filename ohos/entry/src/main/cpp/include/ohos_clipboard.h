/**
 * @file ohos_clipboard.h
 * @brief HarmonyOS 剪贴板实现
 * @author clk
 */

#pragma once

#include <napi/native_api.h>

namespace spiration {

/**
 * @brief 设置剪贴板 NAPI 环境
 * @param env NAPI 环境指针
 */
void set_clipboard_napi_env(napi_env env);

} // namespace spiration
