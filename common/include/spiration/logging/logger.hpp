/**
 * @file logger.h
 * @brief 日志器定义。
 * @author 陈林锴
 */

#pragma once

#include <memory>
#include <spiration/logging/log.h>
#include <spiration/logging/sink.h>
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
     * @brief 记录消息。
     * @param format 格式化消息
     * @param arguments 消息参数
     */
    void log(log message);

private:
    std::vector<std::unique_ptr<sink>> sinks_;
};

}
}
