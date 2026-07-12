/**
 * @file console.h
 * @brief 统一控制台输出与日志管理工具。
 * @author clk
 */

#pragma once

#include <cstdio>
#include <cstdarg>

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
     * @brief 输出调试日志。
     */
    static void debug(const char* format, ...);

    /**
     * @brief 输出普通信息日志。
     */
    static void info(const char* format, ...);

    /**
     * @brief 输出警告日志。
     */
    static void warning(const char* format, ...);

    /**
     * @brief 输出错误日志。
     */
    static void error(const char* format, ...);

private:
    static log_level s_level;
    static void vprint(const char* prefix, const char* format, va_list args);
};

} 
