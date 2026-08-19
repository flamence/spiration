/**
 * @file openai_provider.h
 * @brief OpenAI 兼容对话补全 provider（Chat Completions API）。
 * @author clk
 */

#pragma once

#include <spiration/extension/builtin/agent/web_provider.h>

#include <string>
#include <utility>
#include <vector>

namespace spiration {
namespace agent {

/// @brief OpenAI Chat Completions provider。
class openai_provider : public web_provider {
public:
    std::string name() const override { return "openai"; }

    std::string api_path() const override { return "/chat/completions"; }

    std::pair<std::string, std::string>
    authorization_header(const std::string& api_key) const override {
        return {"Authorization", "Bearer " + api_key};
    }

    // ---- 协议钩子 ----
    void build_reasoning(web_context& ctx, reasoning_level level) const override;
    void build_tools(web_context& ctx, const std::vector<tool_definition>& tools) const override;
    void build_messages(web_context& ctx) const override;
    provider_response parse_response(const std::string& body) const override;
    void handle_stream_line(const std::string& line, web_context& ctx) const override;
};

} // namespace agent
} // namespace spiration
