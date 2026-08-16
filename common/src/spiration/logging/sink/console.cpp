/**
 * @file console.cpp
 * @brief 终端接收器实现。
 * @author clk
 */

#include <iostream>
#include <spiration/logging/console_sink.h>

namespace spiration {
namespace logging {

void console_sink::write(const std::string& message, bool end_line) {
    std::cout << message;
    if (end_line) {
        std::cout << std::endl;
    }
}

}
}
