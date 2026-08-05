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

/// @brief 思考挡位。
enum class reasoning_level {
    none,     ///< 无（不思考）
    low,      ///< 低
    medium,   ///< 标准
    high,     ///< 高
    xhigh,    ///< 超高
    max,      ///< 最大
};

/// @brief 将思考挡位转为字符串标识。
inline const char* reasoning_level_to_string(reasoning_level l) {
    switch (l) {
        case reasoning_level::none:   return "none";
        case reasoning_level::low:    return "low";
        case reasoning_level::medium: return "medium";
        case reasoning_level::high:   return "high";
        case reasoning_level::xhigh:  return "xhigh";
        case reasoning_level::max:    return "max";
    }
    return "medium";
}

/// @brief 从字符串解析思考挡位（兼容旧值 standard→medium、deep→high）。
inline reasoning_level reasoning_level_from_string(const std::string& s) {
    if (s == "none")   return reasoning_level::none;
    if (s == "low")    return reasoning_level::low;
    if (s == "medium") return reasoning_level::medium;
    if (s == "high")   return reasoning_level::high;
    if (s == "xhigh")  return reasoning_level::xhigh;
    if (s == "max")    return reasoning_level::max;
    if (s == "standard") return reasoning_level::medium;  // 兼容旧配置
    if (s == "deep")     return reasoning_level::high;    // 兼容旧配置
    return reasoning_level::medium;
}

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
    reasoning_level reasoning = reasoning_level::medium;
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

    /**
     * @brief 支持的思考挡位列表（按显示顺序）。
     *        不同 provider / 模型对思考能力的支持不同，UI 以此生成可选挡位。
     */
    virtual std::vector<reasoning_level> supported_reasoning_levels() const {
        return {reasoning_level::none, reasoning_level::low, reasoning_level::medium,
                reasoning_level::high, reasoning_level::xhigh, reasoning_level::max};
    }

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
