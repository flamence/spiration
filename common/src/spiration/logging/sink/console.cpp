/**
 * @file console.cpp
 * @brief 终端接收器实现。
 * @author clk
 */

#include <iostream>
#include <spiration/logging/console_sink.h>

namespace spiration {
namespace logging {

void console_sink::write(const log& log, log_formatter formatter) {
    std::cout << formatter.format(log);
}

}
}
