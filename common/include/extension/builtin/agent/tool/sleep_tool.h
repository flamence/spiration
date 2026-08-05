/**
 * @file sleep_tool.h
 * @brief 休眠工具。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/tool/tool.h>

namespace spiration {
namespace agent {

/// @brief 休眠指定秒数。
class sleep_tool : public tool {
public:
    std::string name() const override { return "sleep"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;

    /// @brief 休眠天然耗时较长，兜底超时放宽到 10 分钟。
    long default_timeout_seconds() const override { return 600; }
};

} // namespace agent
} // namespace spiration
