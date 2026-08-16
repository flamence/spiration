/**
 * @file logger.hpp
 * @brief 日志格式化器实现。
 * @author 陈林锴
 */

#pragma once

#include <format>
#include <spiration/logging/log.h>

namespace spiration {
namespace logging {

class log_formatter {
public:
    std::string format(const log& log) {
        std::string formatted_level;
        switch (log.level) {
            case trace:
                formatted_level = "TRACE";
                break;
            case debug:
                formatted_level = "DEBUG";
                break;
            case info:
                formatted_level = "INFO";
                break;
            case warning:
                formatted_level = "WARN";
                break;
            case error:
                formatted_level = "ERROR";
                break;
        }
        return std::format("[{}] [{}] {}{}", formatted_level, log.tag, log.message, log.end_line ? '\n' : '\0');
    }
};

}
}
