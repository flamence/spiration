/**
 * @file sink.cpp
 * @brief 接收器实现。
 * @author clk
 */

#include <spiration/logging/sink.h>

namespace spiration {
namespace logging {

void sink::write_line(const std::string& message) {
    return write(message, true);
}

}
}
