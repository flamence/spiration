/**
 * @file sink.h
 * @brief 接收器定义。
 * @author clk
 */

#pragma once

#include <string>

namespace spiration {
namespace logging {

/**
 * @brief 接收器。
 */
class sink {
public:
    virtual ~sink() = default;

    virtual void write(const std::string& message, bool end_line = false) = 0;

    void write_line(const std::string& message);
};

}
}
