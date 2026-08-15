/**
 * @file platform.cpp
 * @brief 平台工具实现。
 * @author clk
 */

#include <filesystem>
#include <spiration/utils/platform.h>

namespace spiration {

std::filesystem::path platform::temp_directory() {
    return std::filesystem::temp_directory_path() / "spiration";
}

}