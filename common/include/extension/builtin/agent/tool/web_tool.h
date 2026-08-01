/**
 * @file web_tool.h
 * @brief 网络工具集。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/tool/tool.h>
#include <string>

namespace spiration {
namespace agent {

/// @brief 拉取网址内容。
class fetch_tool : public tool {
public:
    std::string name() const override { return "fetch"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
};

} // namespace agent
} // namespace spiration
