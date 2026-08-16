/**
 * @file file.cpp
 * @brief 文件接收器实现。
 * @author clk
 */

#include <spiration/logging/file_sink.h>

namespace spiration {
namespace logging {

file_sink::~file_sink() {
    file_.close();
}

void file_sink::write(const std::string& message, bool end_line) {
    file_ << message;
    if (end_line) {
        file_ << std::endl;
    }
}

}
}
