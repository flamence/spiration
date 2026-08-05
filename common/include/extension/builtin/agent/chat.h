/**
 * @file chat.h
 * @brief 对话客户端。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/provider.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

/// @brief 流式/执行事件回调集合。
struct chat_events {
    std::function<void(const std::string& delta)> on_delta;
    std::function<void(const std::string& delta)> on_reasoning_delta;  ///< 思考过程增量
    std::function<void(const tool_call& tc)> on_tool_call;
    std::function<void(const tool_execution& ex)> on_tool_result;
    std::function<bool()> should_stop;
    std::function<bool()> should_yield;
    /**
     * @brief 敏感工具执行前征询用户批准。
     * @return true 批准执行，false 拒绝。
     */
    std::function<bool(const tool_call&)> on_approve;
};

/**
 * @brief 对话客户端。
 */
class chat_client {
public:
    struct config {
        std::string endpoint;
        std::string api_key;
        std::string model;
        std::string provider = "openai";
        int max_tokens = 0;
        float temperature = 0.7f;
        bool stream = true;
        reasoning_level reasoning = reasoning_level::standard;
        long timeout_seconds = 120;
    };

    explicit chat_client(const config& cfg);
    ~chat_client();

    /// @brief 运行时指定模型。
    void set_model(const std::string& model) { cfg_.model = model; }

    /// @brief 用完整配置替换当前配置并重建 provider。
    void configure(const config& c);

    /// @brief 运行时指定思考等级。
    void set_reasoning_level(reasoning_level level) { cfg_.reasoning = level; }

    /// @brief 获取当前思考等级。
    reasoning_level reasoning_level() const { return cfg_.reasoning; }

    /// @brief 设置 provider。
    void set_provider(std::unique_ptr<provider> p) { provider_ = std::move(p); }

    /// @brief 获取当前 provider 名称。
    std::string provider_name() const;

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

    /// @brief 获取当前对话历史。
    std::vector<chat_message> history() const;

    /// @brief 累计输入 token 数。
    long long input_tokens() const { return total_input_; }
    /// @brief 累计输出 token 数。
    long long output_tokens() const { return total_output_; }

    /// @brief 每次 provider 响应更新 token 计数后回调。
    std::function<void()> on_tokens_updated;

    /// @brief 重置 token 计数。
    void reset_tokens() {
        total_input_ = 0;
        total_output_ = 0;
        if (on_tokens_updated) on_tokens_updated();
    }

    /// @brief 恢复 token 计数。
    void set_tokens(long long in, long long out) {
        total_input_ = in;
        total_output_ = out;
        if (on_tokens_updated) on_tokens_updated();
    }

    /// @brief 按名称切换 provider。
    bool switch_provider(const std::string& name);

    /// @brief 等待所有已启动的工具执行结束。
    void wait_all_tools();

private:
    config cfg_;
    std::unique_ptr<provider> provider_;

    /// @brief 保护对话历史。
    mutable std::mutex history_mtx_;
    std::vector<chat_message> history_;
    std::vector<tool*> tools_;

    /// @brief 正在执行的工具任务数。
    std::atomic<int> active_tool_count_{0};
    std::mutex active_tools_mtx_;
    std::condition_variable active_tools_cv_;

    long long total_input_ = 0;
    long long total_output_ = 0;

    /// @brief 清理不合规的历史消息。
    void sanitize_history();
    std::string http_post(const std::string& url, const std::string& body,
                          const std::vector<std::pair<std::string, std::string>>& headers) const;
    /// @brief 按名称查找工具。
    tool* find_tool(const std::string& name) const;
    /// @brief 在独立线程启动工具执行并返回 future。
    std::future<std::string> launch_tool(const tool_call& tc,
                                         const std::function<bool()>& should_stop);
    /// @brief 执行工具。
    std::string execute_tool(const tool_call& tc,
                             const std::function<bool()>& should_stop);
    chat_response send_stream(const chat_events& events);
    void append_assistant(const chat_response& resp);
};

} // namespace agent
} // namespace spiration
