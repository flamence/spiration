/**
 * @file logger.cpp
 * @brief 日志器实现。
 * @author 陈林锴
 */

#include <spiration/logging/logger.hpp>

namespace spiration {
namespace logging {

logger::logger() {
    level = level::info;
}

logger::~logger() {
    sinks_.clear();
}

template <class... T>
void logger::debug(const std::format_string<T...> format, T&&... arguments) {
    log(level::debug, std::format(format, std::forward<T>(arguments)...));
}

template <class... T>
void logger::error(const std::format_string<T...> format, T&&... arguments) {
    log(level::error, std::format(format, std::forward<T>(arguments)...));
}

template <class... T>
void logger::info(const std::format_string<T...> format, T&&... arguments) {
    log(level::info, std::format(format, std::forward<T>(arguments)...));
}

void logger::log(logging::level level, const std::string message) {
    if (level < this->level) {
        return;
    }
    for (auto& sink : sinks_) {
        sink->write_line(std::format("[{}] {}", std::to_string(level), message));
    }
}

template <class... T>
void logger::trace(const std::format_string<T...> format, T&&... arguments) {
    log(level::trace, std::format(format, std::forward<T>(arguments)...));
}

template <class... T>
void logger::warn(const std::format_string<T...> format, T&&... arguments) {
    log(level::warning, std::format(format, std::forward<T>(arguments)...));
}

}
}
