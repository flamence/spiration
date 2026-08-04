/**
 * @file chat.h
 * @brief OpenAI 兼容的对话客户端。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/tool/tool.h>

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace spiration {
namespace agent {

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
    none,    ///< 无
    standard, ///< 标准
    deep,    ///< 深度
};

/// @brief 单次对话补全响应。
struct chat_response {
    std::string content;
    std::string reasoning_content;  ///< 模型思考过程
    std::vector<tool_call> tool_calls;
    std::string finish_reason;
    std::vector<tool_execution> executions;
};

/// @brief 对话消息。
struct chat_message {
    std::string role;
    std::string content;
    std::string tool_call_id;
    std::string name;
    std::vector<tool_call> tool_calls;
};

/// @brief 流式/执行事件回调集合。
struct chat_events {
    std::function<void(const std::string& delta)> on_delta;
    std::function<void(const std::string& delta)> on_reasoning_delta;  ///< 思考过程增量
    std::function<void(const tool_call& tc)> on_tool_call;
    std::function<void(const tool_execution& ex)> on_tool_result;
    std::function<bool()> should_stop;   ///< 返回 true 时中断 run（停止按钮）
    std::function<bool()> should_yield;  ///< 返回 true 时当前轮结束后不再继续（补充消息到达）
};

/**
 * @brief OpenAI 兼容的对话客户端。
 */
class chat_client {
public:
    struct config {
        std::string endpoint = "https://api.openai.com/v1/completions";
        std::string api_key;
        std::string model;
        int max_tokens = 4096;
        float temperature = 0.7f;
        bool stream = true;
        reasoning_level reasoning = reasoning_level::standard;
    };

    explicit chat_client(const config& cfg);
    ~chat_client();

    /// @brief 运行时指定模型。
    void set_model(const std::string& model) { cfg_.model = model; }

    /// @brief 运行时指定思考等级。
    void set_reasoning_level(reasoning_level level) { cfg_.reasoning = level; }

    /// @brief 获取当前思考等级。
    reasoning_level reasoning_level() const { return cfg_.reasoning; }

    /// @brief 设置系统提示词。
    void set_system_prompt(const std::string& prompt);

    /// @brief 添加一条消息到对话历史。
    void add_message(const chat_message& msg);

    /// @brief 清空对话历史。
    void clear_history();

    /// @brief 注册一个可用工具。工具对象由调用方持有，须比本客户端存活更久。
    void register_tool(tool* t);

    /// @brief 清空已注册的工具列表。
    void clear_tools();

    /// @brief 发送单轮对话补全请求。返回助手的回复。
    chat_response send();

    /**
     * @brief 发送请求并自动执行工具调用循环。
     * @param max_rounds 最大轮数，默认 10。
     * @param events 流式/执行事件回调。
     * @return 最后一轮响应。
     */
    chat_response run(int max_rounds = 10, const chat_events& events = {});

    /// @brief 获取当前配置。
    const config& get_config() const { return cfg_; }

private:
    config cfg_;
    std::vector<chat_message> history_;
    std::vector<tool*> tools_;

    std::string build_request_body() const;
    /// @brief 清理不合规的历史消息（发送前调用）。
    void sanitize_history();
    chat_response parse_response(const std::string& body) const;
    std::string http_post(const std::string& url, const std::string& body,
                          const std::string& api_key) const;
    std::string execute_tool(const tool_call& tc);
    chat_response send_stream(const chat_events& events);
    void append_assistant(const chat_response& resp);
};

} // namespace agent
} // namespace spiration
