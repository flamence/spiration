/**
 * @file path.cpp
 * @brief 路径实现。
 * @author clk
 */

#include <utils/path.h>

#include <string>

namespace spiration {

std::filesystem::path path::u8path(const std::string& s) {
    return std::filesystem::path(s);
}

} // namespace spiration
