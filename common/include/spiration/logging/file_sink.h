/**
 * @file file.h
 * @brief 文件接收器定义。
 * @author clk
 */

#pragma once

#include <ghc/filesystem.hpp>
#include <fstream>
#include <spiration/logging/sink.h>

namespace spiration {
namespace logging {

/**
 * @brief 文件接收器。
 */
class file_sink : public sink {
public:
    explicit file_sink(const ghc::filesystem::path& path) : file_(path, std::ios::app) {}

    ~file_sink() override;
    
    void write(const log& log, log_formatter formatter) override;

private:
    std::ofstream file_;
};

}
}
