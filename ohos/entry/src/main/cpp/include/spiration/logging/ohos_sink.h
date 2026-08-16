/**
 * @file ohos_sink.h
 * @brief 终端接收器定义。
 * @author clk
 */

#pragma once

#include <spiration/logging/sink.h>

namespace spiration {
namespace logging {

/**
 * @brief 终端接收器。
 */
class ohos_sink : public sink {
public:
    void write(const log& message, log_formatter _) override;
};

}
}
