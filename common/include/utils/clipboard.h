/**
 * @file clipboard.h
 * @brief 跨平台剪贴板工具。
 * @author clk
 */

#pragma once

#include <string>

namespace spiration {

/**
 * @brief 剪贴板工具（系统剪贴板读写）。
 *
 * Windows 使用 Win32 剪贴板；macOS 使用 pbcopy/pbpaste；
 * Linux 使用 xclip（需安装 xclip）。
 */
class clipboard {
public:
    /// @brief 将文本写入系统剪贴板。
    static void copy(const std::string& text);

    /// @brief 从系统剪贴板读取文本。
    static std::string paste();

private:
    clipboard() = delete;
};

} // namespace spiration
