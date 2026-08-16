/**
 * @file console.h
 * @brief 终端接收器定义。
 * @author clk
 */

#pragma once

#include <spiration/logging/log.h>
#include <spiration/logging/sink.h>

namespace spiration {
namespace logging {

/**
 * @brief 终端接收器。
 */
class console_sink : public sink {
public:
    void write(const log& log, log_formatter formatter) override;
};

}
}
