/**
 * @file provider.h
 * @brief 对话补全 provider 抽象接口。
 * @author clk
 */

#pragma once

#include <spiration/extension/builtin/agent/tool/tool.hpp>

#include <functional>
#include <memory>
#include <string>
#include <tl/optional.hpp>
#include <vector>

namespace spiration {
namespace agent {

class provider;           // 前向声明
class provider_context;   // 前向声明

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
    tl::optional<std::string> content;
    tl::optional<std::string> reasoning_content;
    std::vector<tool_call> tool_calls;
    std::string finish_reason;
    std::vector<tool_execution> executions;
};

/// @brief 对话消息。
struct chat_message {
    std::string role;
    tl::optional<std::string> content;
    tl::optional<std::string> reasoning_content;
    tl::optional<std::string> tool_call_id;
    tl::optional<std::string> name;
    std::vector<tool_call> tool_calls;
};

/// @brief 发送给 provider 的补全请求。
struct provider_request {
    std::string model;
    std::string endpoint;          ///< 传输端点（网络 provider 使用）。
    std::string api_key;           ///< 鉴权密钥（网络 provider 使用）。
    long timeout_seconds = 120;    ///< 传输超时。
    int max_tokens = 0;
    float temperature = 0.7f;
    bool stream = false;
    reasoning_level reasoning = reasoning_level::medium;
    std::vector<chat_message> messages;
    std::vector<tool_definition> tools;
};

/// @brief provider 解析出的补全响应。
struct provider_response {
    tl::optional<std::string> content;
    tl::optional<std::string> reasoning_content;
    std::vector<tool_call> tool_calls;
    std::string finish_reason;
    int prompt_tokens = 0;
    int completion_tokens = 0;
};

/**
 * @brief 一次补全的执行上下文。
 *        由 provider::build_context() 构造，交由 provider::execute() 处理/发送。
 *        封装请求规格与流式输出回调；provider 可派生子类持有传输/累积状态。
 */
class provider_context {
public:
    explicit provider_context(const provider_request& req) : request_(req) {}
    virtual ~provider_context() = default;

    /// @brief 请求规格。
    const provider_request& request() const { return request_; }

    /// @brief 内容增量回调（流式输出）。
    std::function<void(const std::string&)> on_delta;
    /// @brief 思考过程增量回调（流式输出）。
    std::function<void(const std::string&)> on_reasoning_delta;
    /// @brief 中止检查（返回 true 时停止生成/传输）。
    std::function<bool()> should_stop;

private:
    provider_request request_;
};

/**
 * @brief 对话补全 provider 抽象接口。
 *        面向“对话补全”领域建模，不耦合任何传输实现（HTTP / 进程内本地推理）：
 *          - build_context()：由请求规格构造一次补全的上下文（含思考挡位、工具装配）；
 *          - execute()：处理/发送上下文，产出补全响应，流式输出经上下文回调增量下发；
 *        思考过程与工具调用以结构化字段（provider_response::reasoning_content /
 *        tool_calls）与上下文回调形式显式暴露。
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

    /**
     * @brief 构造一次补全的上下文。
     *        子类将请求规格转换为自身所需的内部状态：
     *          - 网络 provider：端点/鉴权/请求体等；
     *          - 本地 provider：模型参数、提示词等。
     * @param req 补全请求
     * @return 上下文；失败返回 nullptr。
     */
    virtual std::unique_ptr<provider_context> build_context(const provider_request& req) const = 0;

    /**
     * @brief 处理/发送上下文，产出补全响应。
     *        流式输出通过 ctx.on_delta / ctx.on_reasoning_delta 回调增量下发，
     *        最终响应统一由返回的 provider_response 承载（思考过程、工具调用均在结构化字段中）。
     * @param ctx 由 build_context() 构造的上下文
     * @return 补全响应；失败返回空响应。
     */
    virtual provider_response execute(provider_context& ctx) const = 0;
};

} // namespace agent
} // namespace spiration
