/**
 * @file crash_log.h
 * @brief 崩溃日志接口。
 * @author clk
 */

#pragma once

#include <ghc/filesystem.hpp>

namespace spiration {
namespace crash_log {

/**
 * @brief 安装崩溃处理器。
 * @param path 日志文件完整路径。
 */
void install(const ghc::filesystem::path& path);

} // namespace crash_log
} // namespace spiration
