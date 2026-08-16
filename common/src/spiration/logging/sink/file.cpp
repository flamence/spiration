/**
 * @file file.cpp
 * @brief 文件接收器实现。
 * @author clk
 */

#include <spiration/logging/file_sink.h>
#include <spiration/logging/log_formatter.hpp>

namespace spiration {
namespace logging {

file_sink::~file_sink() {
    file_.close();
}

void file_sink::write(const log& message, log_formatter formatter) {
    file_ << formatter.format(message);
    file_.flush();
}

}
}
