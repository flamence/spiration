/**
 * @file console.h
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
class console_sink : public sink {
public:
    void write(const std::string& message, bool end_line = false) override;
};

}
}
