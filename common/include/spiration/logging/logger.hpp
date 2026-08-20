/**
 * @file logger.h
 * @brief 日志器定义。
 * @author 陈林锴
 */

#pragma once

#include <memory>
#include <mutex>
#include <spiration/logging/log.h>
#include <spiration/logging/sink.h>
#include <type_traits>
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
     * @param s 接收器
     */
    template <typename T>
    void add_sink(std::unique_ptr<T> s) {
        static_assert(std::is_base_of<sink, T>::value,
                      "T 必须是 spiration::logging::sink 的派生类型");
        std::lock_guard<std::mutex> lk(mtx_);
        sinks_.emplace_back(std::move(s));
    }

    /**
     * @brief 记录消息。
     * @param format 格式化消息
     * @param arguments 消息参数
     */
    void log(log message);

private:
    mutable std::mutex mtx_;
    std::vector<std::unique_ptr<sink>> sinks_;
};

}
}
