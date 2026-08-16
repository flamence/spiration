/**
 * @file logger.h
 * @brief 日志器定义。
 * @author 陈林锴
 */

#pragma once

#include <format>
#include <memory>
#include <spiration/logging/level.h>
#include <spiration/logging/sink.h>
#include <string>
#include <vector>

namespace spiration {
namespace logging {

/**
 * @brief 日志器。
 */
class logger {
public:
    level level; ///< 日志等级。

    logger();
    ~logger();
    
    /**
     * @brief 添加接收器。
     * @param sink 接收器
     */
    template <std::derived_from<sink> T>
    void add_sink(std::unique_ptr<T> sink) {
        sinks_.emplace_back(std::move(sink));
    }

    /**
     * @brief 记录调试消息。
     * @param format 格式化消息
     * @param arguments 消息参数
     */
    template <class... T>
    void debug(const std::format_string<T...> format, T&&... arguments);
    
    /**
     * @brief 记录错误消息。
     * @param format 格式化消息
     * @param arguments 消息参数
     */
    template <class... T>
    void error(const std::format_string<T...> format, T&&... arguments);

    /**
     * @brief 记录信息消息。
     * @param format 格式化消息
     * @param arguments 消息参数
     */
    template <class... T>
    void info(const std::format_string<T...> format, T&&... arguments);

    /**
     * @brief 记录追溯消息。
     * @param format 格式化消息
     * @param arguments 消息参数
     */
    template <class... T>
    void trace(const std::format_string<T...> format, T&&... arguments);

    /**
     * @brief 记录警告消息。
     * @param format 格式化消息
     * @param arguments 消息参数
     */
    template <class... T>
    void warn(const std::format_string<T...> format, T&&... arguments);

private:
    std::vector<std::unique_ptr<sink>> sinks_;

    void log(logging::level level, const std::string message);
};

}
}
