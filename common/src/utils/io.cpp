/**
 * @file io.h
 * @author clk
 * @brief I/O 工具实现。
 */

#include <fstream>
#include <utils/io.h>

namespace spiration {
namespace io {

std::string read_file_content(const std::filesystem::path& filepath) {
    if (!std::filesystem::exists(filepath) || !std::filesystem::is_regular_file(filepath)) {
        throw std::runtime_error("file does not exist or is not a regular file");
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace io
} // namespace spiration
