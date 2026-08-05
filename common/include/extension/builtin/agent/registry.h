/**
 * @file registry.h
 * @brief 智能体注册表。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/provider.h>
#include <extension/builtin/agent/tool/tool.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

/**
 * @brief 智能体拓展注册表。
 */
class agent_registry {
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

    /// @brief 服务名称。
    static constexpr const char* SERVICE_NAME = "agent.registry";

private:
    agent_registry() = default;
    mutable std::mutex mtx_;
    std::map<std::string, std::function<std::unique_ptr<provider>()>> providers_;
    std::vector<std::unique_ptr<tool>> tools_;
};

} // namespace agent
} // namespace spiration
