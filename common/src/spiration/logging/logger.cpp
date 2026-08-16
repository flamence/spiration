/**
 * @file logger.cpp
 * @brief 日志器实现。
 * @author 陈林锴
 */

#include <spiration/logging/logger.hpp>

namespace spiration {
namespace logging {
namespace {

log_formatter log_formatter_ = log_formatter();

}

logger::logger() {
    level = level::info;
}

logger::~logger() {
    sinks_.clear();
}

void logger::log(logging::log log) {
    if (log.level < this->level) {
        return;
    }
    for (auto& sink : sinks_) {
        sink->write(log, log_formatter_);
    }
}

}
}
