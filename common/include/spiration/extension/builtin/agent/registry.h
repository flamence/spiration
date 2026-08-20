/**
 * @file registry.h
 * @brief 智能体注册表。
 * @author clk
 */

#pragma once

#include <spiration/export.h>
#include <spiration/extension/builtin/agent/chat.h>
#include <spiration/extension/builtin/agent/provider.h>
#include <spiration/extension/builtin/agent/tool/tool.hpp>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

/// @brief 模型选项。
struct model_option {
    std::string display_name;
    chat_client::config cfg;
};

/// @brief 模型来源。
using model_source = std::function<std::vector<model_option>()>;

/**
 * @brief 智能体拓展注册表。
 */
class SPIRATION_EXPORT agent_registry {
public:
    static agent_registry& instance();

    /**
     * @brief 注册 provider 工厂。
     * @param name    provider 名称
     * @param factory 返回 provider 实例的工厂
     */
    void register_provider(const std::string& name,
                           std::function<std::unique_ptr<provider>()> factory);

    /**
     * @brief 创建 provider。
     */
    std::unique_ptr<provider> create_provider(const std::string& name);

    /// @brief 已注册的 provider 名称。
    std::vector<std::string> provider_names() const;

    /// @brief 注册工具。
    void register_tool(std::unique_ptr<tool> t);

    /// @brief 全部已注册工具。
    std::vector<tool*> tools() const;

    /**
     * @brief 注册模型来源（其他拓展向智能体提供可选模型）。
     */
    void register_model_source(model_source source);

    /**
     * @brief 汇总全部模型来源提供的模型。
     * @return 各来源返回模型的并集（不合并智能体自身的 models.json 配置）。
     */
    std::vector<model_option> collect_models() const;

    /**
     * @brief 订阅"模型来源变更"通知。
     * @param cb 回调
     * @return 订阅 ID（用于取消，当前不提供取消接口，按拓展生命周期存续）
     */
    int on_models_changed(std::function<void()> cb);

    /**
     * @brief 发布"模型来源变更"通知（模型来源在增删模型后调用）。
     */
    void notify_models_changed();

    /// @brief 服务名称。
    static constexpr const char* SERVICE_NAME = "agent.registry";

private:
    agent_registry();
    mutable std::mutex mtx_;
    std::map<std::string, std::function<std::unique_ptr<provider>()>> providers_;
    std::vector<std::unique_ptr<tool>> tools_;
    std::vector<model_source> model_sources_;
    std::map<int, std::function<void()>> models_changed_cbs_;
    int next_cb_id_ = 1;
};

} // namespace agent
} // namespace spiration
