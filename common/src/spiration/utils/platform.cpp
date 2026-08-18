/**
 * @file platform.cpp
 * @brief 平台工具实现。
 * @author clk
 */

#include <ghc/filesystem.hpp>
#include <spiration/utils/platform.h>

namespace spiration {

ghc::filesystem::path platform::temp_directory() {
    return ghc::filesystem::temp_directory_path() / "spiration";
}

}