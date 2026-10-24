/**
 * @file registry.cpp
 * @brief 智能体注册表实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/builtin/agent/registry.h>
#include <spiration/utils/dispatcher.h>

#include <exception>
#include <fmt/format.h>

namespace spiration {
namespace agent {

agent_registry& agent_registry::instance() {
    static agent_registry inst;
    return inst;
}

agent_registry::agent_registry() {
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
    if (!factory) return nullptr;
    try {
        std::unique_ptr<provider> p = factory();
        if (p) return p;
    } catch (const std::exception& e) {
        application::instance()->logger()->log({logging::level::error, fmt::format("provider \"{}\" factory threw: {}", name, e.what()), "extension/agent/registry"});
    }
    return nullptr;
}

std::vector<std::string> agent_registry::provider_names() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> names;
    names.reserve(providers_.size());
    for (const auto& p : providers_) names.push_back(p.first);
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

void agent_registry::register_model_source(model_source source) {
    if (!source) return;
    std::lock_guard<std::mutex> lk(mtx_);
    model_sources_.push_back(std::move(source));
}

std::vector<model_option> agent_registry::collect_models() const {
    std::vector<model_source> sources;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        sources = model_sources_;
    }
    std::vector<model_option> out;
    for (auto& src : sources) {
        try {
            auto models = src();
            for (auto& m : models) out.push_back(std::move(m));
        } catch (const std::exception& e) {
            application::instance()->logger()->log(
                {logging::level::error,
                 fmt::format("model source threw: {}", e.what()),
                 "extension/agent/registry"});
        }
    }
    return out;
}

int agent_registry::on_models_changed(std::function<void()> cb) {
    if (!cb) return 0;
    std::lock_guard<std::mutex> lk(mtx_);
    int id = next_cb_id_++;
    models_changed_cbs_[id] = std::move(cb);
    return id;
}

void agent_registry::off_models_changed(int id) {
    std::lock_guard<std::mutex> lk(mtx_);
    models_changed_cbs_.erase(id);
}

void agent_registry::notify_models_changed() {
    std::vector<std::function<void()>> cbs;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& kv : models_changed_cbs_) cbs.push_back(kv.second);
    }
    // 回调会刷新 UI 控件树，统一投递到 UI 线程执行，避免后台线程（模型加载等）触碰控件。
    auto run = [](const std::function<void()>& cb) {
        try {
            cb();
        } catch (const std::exception& e) {
            application::instance()->logger()->log(
                {logging::level::error,
                 fmt::format("models_changed callback threw: {}", e.what()),
                 "extension/agent/registry"});
        }
    };
    if (auto* d = application::instance()->dispatcher()) {
        for (auto& cb : cbs) {
            d->post_to_ui([cb, run]() { run(cb); });
        }
    } else {
        for (auto& cb : cbs) {
            run(cb);
        }
    }
}

} // namespace agent
} // namespace spiration
