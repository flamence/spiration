/**
 * @file ohos_clipboard.h
 * @brief HarmonyOS 剪贴板实现
 * @author clk
 */

#pragma once

#include <napi/native_api.h>
#include <string>

namespace spiration {

/**
 * @brief 调用 ArkTS 侧注册的复制回调。
 * @param text 待复制文本
 * @return 回调已注册并调用成功
 */
bool clipboard_copy_to_arkts(const std::string& text);

/**
 * @brief 调用 ArkTS 侧注册的粘贴回调。
 * @return 剪贴板文本；未注册或失败返回空串
 */
std::string clipboard_paste_from_arkts();

} // namespace spiration
