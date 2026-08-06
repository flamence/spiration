/**
 * @file crash_log.h
 * @brief 崩溃日志接口。
 * @author clk
 */

#pragma once

#include <string>

namespace spiration {
namespace crash_log {

/**
 * @brief 安装崩溃处理器。崩溃时将异常/信号与调用栈追加写入指定日志文件。
 * @param path 日志文件完整路径。
 * @note Windows 使用结构化异常处理（SEH）+ DbgHelp 符号化；
 *       POSIX 使用信号处理器 + backtrace_symbols。
 */
void install(const std::string& path);

} // namespace crash_log
} // namespace spiration
