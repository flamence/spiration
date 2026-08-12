/**
 * @file io.h
 * @author clk
 * @brief I/O 工具定义。
 */

#include <filesystem>
#include <string>

namespace spiration {
namespace io {

std::string read_file_content(const std::filesystem::path& filepath);

} // namespace io
} // namespace spiration
