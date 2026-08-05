/**
 * @file console.h
 * @brief 统一控制台输出与日志管理工具。
 * @author clk
 */

#pragma once

#include <cstdio>
#include <cstdarg>
#include <string>

namespace spiration {

/**
 * @brief 日志等级枚举。
 */
enum class log_level {
    debug,   
    info,    
    warning, 
    error    
};

/**
 * @brief 控制台输出管理类。
 */
class console {
public:
    /**
     * @brief 设置当前日志等级，低于此等级的日志将被忽略。
     */
    static void set_level(log_level level);

    /**
     * @brief 获取当前日志等级。
     */
    static log_level get_level();

    /**
     * @brief 设置日志文件路径。
     * @param path 日志文件完整路径；传空串则关闭文件输出。
     * @note 设置后所有 console::* 输出同时写入 stdout 与该文件。
     */
    static void set_log_file(const std::string& path);

    /**
     * @brief 获取当前日志文件路径。
     */
    static std::string log_file_path();

    /**
     * @brief 生成带时间戳的日志文件路径。
     * @param dir 日志目录。
     * @param prefix 文件名前缀。
     * @return "<dir>/<prefix>-YYYYMMDD-HHMMSS.log"。
     */
    static std::string make_log_path(const std::string& dir, const std::string& prefix);

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
    static log_level s_level;
    static void vprint(log_level level, const char* tag, const char* format, va_list args);
};

} 
