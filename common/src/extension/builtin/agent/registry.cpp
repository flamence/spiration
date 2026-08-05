/**
 * @file registry.cpp
 * @brief 智能体注册表实现。
 * @author clk
 */

#include <extension/builtin/agent/registry.h>
#include <utils/console.h>

#include <exception>

namespace spiration {
namespace agent {

agent_registry& agent_registry::instance() {
    static agent_registry inst;
    return inst;
}

void agent_registry::register_provider(
    const std::string& name, std::function<std::unique_ptr<provider>()> factory) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (name.empty() || !factory) return;
    providers_[name] = std::move(factory);
}

std::unique_ptr<provider> agent_registry::create_provider(const std::string& name) {
    std::function<std::unique_ptr<provider>()> factory;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = providers_.find(name);
        if (it != providers_.end()) factory = it->second;
    }
    if (factory) {
        try {
            std::unique_ptr<provider> p = factory();
            if (p) return p;
        } catch (const std::exception& e) {
            console::error("extension/agent/registry",
                           "provider \"%s\" factory threw: %s", name.c_str(), e.what());
        }
    }
    return spiration::agent::create_provider(name);
}

std::vector<std::string> agent_registry::provider_names() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> names;
    names.reserve(providers_.size());
    for (const auto& [name, fn] : providers_) names.push_back(name);
    return names;
}

void agent_registry::register_tool(std::unique_ptr<tool> t) {
    if (!t) return;
    std::lock_guard<std::mutex> lk(mtx_);
    tools_.push_back(std::move(t));
}

std::vector<tool*> agent_registry::tools() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<tool*> out;
    out.reserve(tools_.size());
    for (const auto& t : tools_) out.push_back(t.get());
    return out;
}

} // namespace agent
} // namespace spiration
