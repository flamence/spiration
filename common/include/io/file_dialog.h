/**
 * @file file_dialog.h
 * @brief 文件选择对话框。
 * @author clk
 */

#pragma once

#include <string>
#include <vector>

namespace spiration {
namespace io {

/**
 * @brief 打开文件选择对话框。
 * @param title  对话框标题
 * @param filter 文件筛选器描述
 * @param patterns  匹配模式列表
 * @return 选择的文件完整路径；如果用户取消则返回空字符串
 */
std::string open_file(const std::string& title = "Open File",
                      const std::string& filter = "All Files (*.*)",
                      const std::vector<std::string>& patterns = {"*"});

/**
 * @brief 保存文件对话框。
 * @param title  对话框标题
 * @param filter 文件筛选器描述
 * @param patterns  匹配模式列表
 * @return 选择的文件完整路径；如果用户取消则返回空字符串
 */
std::string save_file(const std::string& title = "Save File",
                      const std::string& filter = "All Files (*.*)",
                      const std::vector<std::string>& patterns = {"*"});

} // namespace io
} // namespace spiration
