/**
 * @file provider.h
 * @brief 对话补全 provider 抽象接口。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/tool/tool.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

class provider;  // 前向声明

/// @brief 一次工具调用。
struct tool_call {
    std::string id;
    std::string function_name;
    std::string arguments;
};

/// @brief 一次已执行的工具调用。
struct tool_execution {
    std::string function_name;
    std::string arguments;
    std::string result;
};

/// @brief 思考等级。
enum class reasoning_level {
    none,     ///< 无
    standard, ///< 标准
    deep,     ///< 深度
};

/// @brief 单次对话补全响应。
struct chat_response {
    std::string content;
    std::string reasoning_content;
    std::vector<tool_call> tool_calls;
    std::string finish_reason;
    std::vector<tool_execution> executions;
};

/// @brief 对话消息。
struct chat_message {
    std::string role;
    std::string content;
    std::string reasoning_content;
    std::string tool_call_id;
    std::string name;
    std::vector<tool_call> tool_calls;
};

/// @brief 发送给 provider 的补全请求。
struct provider_request {
    std::string model;
    int max_tokens = 0;
    float temperature = 0.7f;
    bool stream = false;
    reasoning_level reasoning = reasoning_level::standard;
    std::vector<chat_message> messages;
    std::vector<tool_definition> tools;
};

/// @brief provider 解析出的补全响应。
struct provider_response {
    std::string content;
    std::string reasoning_content;
    std::vector<tool_call> tool_calls;
    std::string finish_reason;
    int prompt_tokens = 0;
    int completion_tokens = 0;
};

/// @brief 流式解析的共享累积状态。
struct stream_context {
    std::string line_buf;
    std::string content;
    std::string reasoning;
    std::string finish_reason;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    std::map<int, tool_call> tc_map;
    std::function<void(const std::string&)> on_delta;
    std::function<void(const std::string&)> on_reasoning_delta;
    std::function<bool()> should_stop;
    const provider* provider = nullptr;
};

/**
 * @brief 对话补全 provider 抽象。
 */
class provider {
public:
    virtual ~provider() = default;

    /// @brief provider 名称。
    virtual std::string name() const = 0;

    /// @brief 基于配置的 endpoint 生成补全请求 URL。
    virtual std::string chat_url(const std::string& endpoint) const = 0;

    /// @brief 构建请求头。
    virtual std::vector<std::pair<std::string, std::string>>
    request_headers(const std::string& api_key) const = 0;

    /// @brief 构建请求体 JSON 字符串。
    virtual std::string build_request_body(const provider_request& req) const = 0;

    /// @brief 解析非流式响应。
    virtual provider_response parse_response(const std::string& body) const = 0;

    /**
     * @brief 处理一条流式行。
     * @param line 原始 SSE 行
     * @param ctx  流式累积状态
     */
    virtual void handle_stream_line(const std::string& line, stream_context& ctx) const = 0;
};

/// @brief 根据名称创建 provider 实例。
std::unique_ptr<provider> create_provider(const std::string& name);

} // namespace agent
} // namespace spiration
