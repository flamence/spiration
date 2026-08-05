/**
 * @file file_dialog.h
 * @brief 文件选择对话框。
 * @author clk
 */

#pragma once

#include <string>
#include <vector>

#if defined(__OHOS__)
#include <functional>
#endif

namespace spiration {
namespace io {

#if defined(OHOS_PLATFORM)

/**
 * @brief 鸿蒙系统异步打开文件对话框。
 * @note 鸿蒙系统文件选择器为异步 UI 流程，无法用同步
 *       open_file 阻塞等待；选择结果经 on_result 回调返回。
 * @param on_result 回调 (uri, content)：uri 为选择文件 URI，
 *        content 为 ArkTS 侧经 fileIo 读取的文件内容。
 */
void open_file_async(const std::string& title, const std::string& filter,
                     const std::vector<std::string>& patterns,
                     std::function<void(const std::string&, const std::string&)> on_result);

/**
 * @brief 鸿蒙系统异步保存文件对话框。
 * @param content 待写入文件的内容。
 * @param on_result 回调 (uri)：写入成功后返回目标 URI，取消为空串。
 */
void save_file_async(const std::string& title, const std::string& filter,
                     const std::vector<std::string>& patterns,
                     const std::string& content,
                     std::function<void(const std::string&)> on_result);

/**
 * @brief 交付文件对话框结果。
 * @param uri     选择的文件 URI；取消为空串。
 * @param content 打开时读取的文件内容。
 */
void deliver_file_dialog_result(const std::string& uri, const std::string& content);

#else

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

#endif

} // namespace io
} // namespace spiration
