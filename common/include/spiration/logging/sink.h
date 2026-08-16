/**
 * @file sink.h
 * @brief 接收器定义。
 * @author clk
 */

#pragma once

#include <spiration/logging/log.h>
#include <spiration/logging/log_formatter.hpp>

namespace spiration {
namespace logging {

/**
 * @brief 接收器。
 */
class sink {
public:
    virtual ~sink() = default;

    virtual void write(const log& log, log_formatter formatter) = 0;
};

}
}
