/**
 * @file console.h
 * @brief 控制台定义。
 * @author clk
 */

#pragma once

#include <cstdarg>
#include <filesystem>
#include <string>

namespace spiration {

/**
 * @brief 日志等级枚举。
 */
enum class log_level {
    debug, ///< 调试
    info, ///< 信息
    warning, ///< 警告
    error ///< 错误
};

/**
 * @brief 控制台类。
 */
class console {
public:
    /**
     * @brief 设置当前日志等级。
     * @param level 最低日志等级。
     */
    static void set_level(log_level level);

    /**
     * @brief 获取当前日志等级。
     */
    static log_level get_level();

    /**
     * @brief 设置日志文件路径。
     * @param path 日志文件路径。
     */
    static void set_log_file(const std::filesystem::path& path);

    /**
     * @brief 获取当前日志文件路径。
     */
    static std::filesystem::path log_file_path();

    /**
     * @brief 生成带时间戳的日志文件路径。
     * @param dir 日志目录。
     * @param prefix 文件名前缀。
     */
    static std::filesystem::path make_log_path(const std::filesystem::path& dir, const std::string& prefix);

    /**
     * @brief 输出调试日志。
     * @param tag 日志标签。
     */
    static void debug(const char* tag, const char* format, ...);

    /**
     * @brief 输出普通信息日志。
     * @param tag 日志标签。
     */
    static void info(const char* tag, const char* format, ...);

    /**
     * @brief 输出警告日志。
     * @param tag 日志标签。
     */
    static void warning(const char* tag, const char* format, ...);

    /**
     * @brief 输出错误日志。
     * @param tag 日志标签。
     */
    static void error(const char* tag, const char* format, ...);

private:
    static log_level level;
    static void vprint(log_level level, const char* tag, const char* format, va_list args);
};

} 
