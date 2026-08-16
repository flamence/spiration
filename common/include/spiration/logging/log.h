/**
 * @file log.h
 * @brief 日志定义。
 * @author clk
 */

#pragma once

#include <spiration/logging/level.h>
#include <string>

namespace spiration {
namespace logging {

/**
 * @brief 日志消息。
 */
struct log {
    level level;
    std::string message;
    std::string tag;
    bool end_line = true;
};

}
}
