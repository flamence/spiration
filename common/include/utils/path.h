/**
 * @file path.h
 * @brief 路径工具。
 * @author clk
 */

#pragma once

#include <filesystem>
#include <string>

namespace spiration {

/**
 * @brief 路径工具。
 */
class path {
public:
    static std::filesystem::path u8path(const std::string& s);

private:
    path() = delete;
};

} // namespace spiration
