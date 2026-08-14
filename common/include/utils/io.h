/**
 * @file io.h
 * @author clk
 * @brief I/O 工具定义。
 */

#include <filesystem>
#include <string>

namespace spiration {
namespace io {

/**
 * @brief 读取文本文件。
 * @param path 文件路径
 * @return 文件内容。
 */
std::string read_file_content(const std::filesystem::path& path);

/**
 * @brief 以文本写入文件。
 * @param path 文件路径
 * @param content 文本内容
 * @return 是否成功写入。
 */
bool write_file_content(const std::filesystem::path& path, const std::string& content);

} // namespace io
} // namespace spiration
