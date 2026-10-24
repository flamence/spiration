/**
 * @file chat.cpp
 * @brief 对话客户端实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/builtin/agent/chat.h>
#include <spiration/extension/builtin/agent/registry.h>
#include <spiration/net/http.h>
#include <spiration/utils/dispatcher.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <exception>
#include <fmt/format.h>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <utility>

namespace spiration {
namespace agent {

chat_client::chat_client(const config& cfg) : cfg_(cfg) {
    provider_ = agent_registry::instance().create_provider(cfg_.provider);
}

void chat_client::configure(const config& c) {
    if (running_.load(std::memory_order_acquire)) {
        application::instance()->logger()->log({logging::level::warning, "configure ignored: a run is in progress", "extension/agent"});
        return;
    }
    cfg_ = c;
    provider_ = agent_registry::instance().create_provider(cfg_.provider);
    application::instance()->logger()->log({logging::level::info, fmt::format("configured: provider={} model={}", cfg_.provider, cfg_.model), "extension/agent"});
}

chat_client::~chat_client() = default;

std::string chat_client::provider_name() const {
    return provider_ ? provider_->name() : cfg_.provider;
}

void chat_client::set_system_prompt(const std::string& prompt) {
    std::lock_guard<std::mutex> lk(history_mtx_);
    history_.erase(
        std::remove_if(history_.begin(), history_.end(),
                       [](const chat_message& m) { return m.role == "system"; }),
        history_.end());
    if (!prompt.empty()) {
        history_.insert(history_.begin(), {"system", prompt, "", "", {}});
    }
}

void chat_client::add_message(const chat_message& msg) {
    std::lock_guard<std::mutex> lk(history_mtx_);
    history_.push_back(msg);
}

void chat_client::clear_history() {
    std::lock_guard<std::mutex> lk(history_mtx_);
    history_.erase(
        std::remove_if(history_.begin(), history_.end(),
                       [](const chat_message& m) { return m.role != "system"; }),
        history_.end());
}

void chat_client::register_tool(tool* t) {
    std::lock_guard<std::mutex> lk(tools_mtx_);
    if (t) tools_.push_back(t);
}

std::vector<chat_message> chat_client::history() const {
    std::lock_guard<std::mutex> lk(history_mtx_);
    return history_;
}

void chat_client::wait_all_tools() {
    std::unique_lock<std::mutex> lk(active_tools_mtx_);
    active_tools_cv_.wait(lk, [this]() {
        return active_tool_count_.load(std::memory_order_acquire) == 0;
    });
}

void chat_client::clear_tools() {
    std::lock_guard<std::mutex> lk(tools_mtx_);
    tools_.clear();
}

chat_response chat_client::send() {
    return send_stream(chat_events{});
}

void chat_client::append_assistant(const chat_response& resp) {
    if (resp.content.has_value() || !resp.tool_calls.empty() ||
        resp.reasoning_content.has_value()) {
        chat_message assistant;
        assistant.role            = "assistant";
        assistant.content         = resp.content;
        assistant.reasoning_content = resp.reasoning_content;
        assistant.tool_calls      = resp.tool_calls;
        std::lock_guard<std::mutex> lk(history_mtx_);
        history_.push_back(assistant);
    }
}

void chat_client::sanitize_history() {
    history_.erase(
        std::remove_if(history_.begin(), history_.end(),
                       [](const chat_message& m) {
                           if (m.role == "system") return false;
                           if (m.role == "assistant")
                               return !m.content.has_value() && m.tool_calls.empty() &&
                                      !m.tool_call_id.has_value() &&
                                      !m.reasoning_content.has_value();
                           return !m.content.has_value();
                       }),
        history_.end());

    for (size_t i = 0; i < history_.size(); ++i) {
        auto& m = history_[i];
        if (m.role != "assistant" || m.tool_calls.empty()) continue;
        bool all_resolved = true;
        for (const auto& tc : m.tool_calls) {
            bool found = false;
            for (size_t j = i + 1; j < history_.size(); ++j) {
                if (history_[j].role == "tool" && history_[j].tool_call_id == tc.id) {
                    found = true;
                    break;
                }
            }
            if (!found) { all_resolved = false; break; }
        }
        if (all_resolved) continue;
        if (!m.content.has_value()) {
            history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(i));
            --i;
        } else {
            m.tool_calls.clear();
        }
    }

    std::set<std::string> valid_ids;
    for (const auto& m : history_) {
        if (m.role == "assistant") {
            for (const auto& tc : m.tool_calls) valid_ids.insert(tc.id);
        }
    }
    history_.erase(
        std::remove_if(history_.begin(), history_.end(),
                       [&](const chat_message& m) {
                           if (m.role != "tool") return false;
                           return !m.tool_call_id.has_value() ||
                                  valid_ids.count(m.tool_call_id.value()) == 0;
                       }),
        history_.end());
}

chat_response chat_client::send_stream(const chat_events& events) {
    if (!provider_) {
        application::instance()->logger()->log({logging::level::error, "no provider configured", "extension/agent"});
        return {};
    }
    std::vector<chat_message> msgs;
    {
        std::lock_guard<std::mutex> lk(history_mtx_);
        sanitize_history();
        msgs = history_;
    }

    provider_request req;
    req.model           = cfg_.model;
    req.endpoint        = cfg_.endpoint;
    req.api_key         = cfg_.api_key;
    req.timeout_seconds = cfg_.timeout_seconds;
    req.max_tokens      = cfg_.max_tokens;
    req.temperature     = cfg_.temperature;
    req.stream          = cfg_.stream;
    req.reasoning       = cfg_.reasoning;
    req.messages        = msgs;
    for (const auto* t : tools_) {
        if (t) req.tools.push_back(t->to_definition());
    }

    // 由 provider 构造上下文（含思考过程与工具装配），再统一处理/发送。
    std::unique_ptr<provider_context> ctx;
    try {
        ctx = provider_->build_context(req);
    } catch (const std::exception& e) {
        application::instance()->logger()->log({logging::level::error, fmt::format("build context failed: {}", e.what()), "extension/agent"});
        return {};
    }
    if (!ctx) {
        application::instance()->logger()->log({logging::level::error, "provider failed to build context", "extension/agent"});
        return {};
    }

    ctx->on_delta = events.on_delta;
    ctx->on_reasoning_delta = events.on_reasoning_delta;
    ctx->should_stop = events.should_stop;

    provider_response pr;
    try {
        pr = provider_->execute(*ctx);
    } catch (const std::exception& e) {
        application::instance()->logger()->log({logging::level::error, fmt::format("provider execute failed: {}", e.what()), "extension/agent"});
        return {};
    }

    return finalize_response(std::move(pr), events);
}

chat_response chat_client::finalize_response(provider_response pr, const chat_events& events) {
    chat_response resp;
    resp.content           = pr.content;
    resp.reasoning_content = pr.reasoning_content;
    resp.tool_calls        = std::move(pr.tool_calls);
    resp.finish_reason     = pr.finish_reason;
    total_input_.fetch_add(pr.prompt_tokens, std::memory_order_relaxed);
    total_output_.fetch_add(pr.completion_tokens, std::memory_order_relaxed);
    if (on_tokens_updated) on_tokens_updated();
    if (events.on_delta && resp.content.has_value())
        events.on_delta(resp.content.value());
    if (events.on_reasoning_delta && resp.reasoning_content.has_value())
        events.on_reasoning_delta(resp.reasoning_content.value());
    append_assistant(resp);
    return resp;
}

tool* chat_client::find_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lk(tools_mtx_);
    for (auto* t : tools_) {
        if (t && t->name() == name) return t;
    }
    return nullptr;
}

bool chat_client::switch_provider(const std::string& name) {
    if (name.empty()) return true;
    if (provider_ && provider_->name() == name) return true;
    if (running_.load(std::memory_order_acquire)) {
        application::instance()->logger()->log({logging::level::warning, "switch provider ignored: a run is in progress", "extension/agent"});
        return false;
    }
    auto p = agent_registry::instance().create_provider(name);
    if (!p) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("switch provider failed: {}", name), "extension/agent"});
        return false;
    }
    provider_ = std::move(p);
    application::instance()->logger()->log({logging::level::info, fmt::format("switched provider to {}", name), "extension/agent"});
    return true;
}

std::future<std::string> chat_client::launch_tool(const tool_call& tc,
                                                  const std::function<bool()>& should_stop) {
    tool* t = find_tool(tc.function_name);
    if (!t) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("tool not found: {}", tc.function_name), "extension/agent"});
        std::promise<std::string> p;
        p.set_value("[error] tool not found: " + tc.function_name);
        return p.get_future();
    }

    if (!t->should_stop) t->should_stop = should_stop;
    application::instance()->logger()->log({logging::level::info, fmt::format("executing tool: {}", tc.function_name), "extension/agent"});

    active_tool_count_.fetch_add(1, std::memory_order_release);
    auto task = std::make_shared<std::packaged_task<std::string()>>(
        [t, args = tc.arguments, this]() {
            std::string result = t->execute(args);
            if (active_tool_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard<std::mutex> lk(active_tools_mtx_);
                active_tools_cv_.notify_all();
            }
            return result;
        });
    std::future<std::string> fut = task->get_future();
    if (auto* d = application::instance()->dispatcher()) {
        d->post([task]() { (*task)(); });
    } else {
        std::thread([task]() { (*task)(); }).detach();
    }
    return fut;
}

std::string chat_client::execute_tool(const tool_call& tc,
                                      const std::function<bool()>& should_stop) {
    tool* t = find_tool(tc.function_name);
    long timeout = t ? t->default_timeout_seconds() : 30;

    std::future<std::string> fut = launch_tool(tc, should_stop);
    if (timeout > 0 && fut.wait_for(std::chrono::seconds(timeout)) == std::future_status::timeout) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("tool '{}' timed out after {}s", tc.function_name, timeout), "extension/agent"});
        return "[error] tool '" + tc.function_name + "' timed out after " +
               std::to_string(timeout) + "s";
    }
    try {
        return fut.get();
    } catch (const std::exception& e) {
        application::instance()->logger()->log({logging::level::error, fmt::format("tool '{}' threw: {}", tc.function_name, e.what()), "extension/agent"});
        return "[error] tool execution failed: " + std::string(e.what());
    }
}

chat_response chat_client::run(const chat_events& events, tl::optional<int> max_rounds) {
    running_.store(true, std::memory_order_release);
    struct run_guard {
        std::atomic<bool>& flag;
        ~run_guard() { flag.store(false, std::memory_order_release); }
    } guard{running_};

    chat_response last;
    std::vector<tool_execution> all_executions;

    constexpr size_t MAX_CONCURRENT = 4;

    for (int round = 0; !max_rounds.has_value() || round < max_rounds.value(); ++round) {
        if (events.should_stop && events.should_stop()) break;
        if (events.should_yield && events.should_yield()) break;
        last = send_stream(events);

        if (last.tool_calls.empty())
            break;

        const size_t n = last.tool_calls.size();
        std::vector<std::string> results(n);
        std::vector<bool> executed(n, false);

        for (size_t i = 0; i < n; ++i) {
            if (events.should_stop && events.should_stop()) break;
            if (events.should_yield && events.should_yield()) break;
            tool* t = find_tool(last.tool_calls[i].function_name);
            if (!t || (!t->serial() && !t->requires_approval())) continue;

            if (events.on_tool_call) events.on_tool_call(last.tool_calls[i]);
            if (t->requires_approval()) {
                bool approved = events.on_approve ? events.on_approve(last.tool_calls[i]) : false;
                if (!approved) {
                    results[i] = "[error] user denied approval for " +
                                 last.tool_calls[i].function_name;
                    executed[i] = true;
                    continue;
                }
            }
            results[i] = execute_tool(last.tool_calls[i], events.should_stop);
            executed[i] = true;
        }

        size_t i = 0;
        while (i < n) {
            if (events.should_stop && events.should_stop()) break;
            if (events.should_yield && events.should_yield()) break;

            tool* t = find_tool(last.tool_calls[i].function_name);
            if (t && (t->serial() || t->requires_approval())) { ++i; continue; }

            std::vector<size_t> batch;
            while (i < n && batch.size() < MAX_CONCURRENT) {
                tool* bt = find_tool(last.tool_calls[i].function_name);
                if (bt && (bt->serial() || bt->requires_approval())) { ++i; continue; }
                batch.push_back(i);
                ++i;
            }

            std::vector<std::future<std::string>> futs;
            futs.reserve(batch.size());
            for (size_t j : batch) {
                if (events.on_tool_call) events.on_tool_call(last.tool_calls[j]);
                futs.push_back(launch_tool(last.tool_calls[j], events.should_stop));
            }
            for (size_t k = 0; k < batch.size(); ++k) {
                if (events.should_stop && events.should_stop()) break;
                size_t j = batch[k];
                tool* bt = find_tool(last.tool_calls[j].function_name);
                long timeout = bt ? bt->default_timeout_seconds() : 30;
                if (timeout > 0 &&
                    futs[k].wait_for(std::chrono::seconds(timeout)) == std::future_status::timeout) {
                    results[j] = "[error] tool '" + last.tool_calls[j].function_name +
                                 "' timed out after " + std::to_string(timeout) + "s";
                } else {
                    try {
                        results[j] = futs[k].get();
                    } catch (const std::exception& e) {
                        results[j] = "[error] tool execution failed: " + std::string(e.what());
                    }
                }
                executed[j] = true;
            }
        }

        for (size_t j = 0; j < n; ++j) {
            if (!executed[j]) continue;
            if (events.should_yield && events.should_yield()) break;

            tool_execution ex;
            ex.function_name = last.tool_calls[j].function_name;
            ex.arguments     = last.tool_calls[j].arguments;
            ex.result        = results[j];
            all_executions.push_back(ex);

            if (events.on_tool_result) events.on_tool_result(ex);

            chat_message tool_msg;
            tool_msg.role         = "tool";
            tool_msg.content      = results[j];
            tool_msg.tool_call_id = last.tool_calls[j].id;
            tool_msg.name         = last.tool_calls[j].function_name;
            {
                std::lock_guard<std::mutex> lk(history_mtx_);
                history_.push_back(std::move(tool_msg));
            }
        }
    }

    last.executions = std::move(all_executions);
    return last;
}

} // namespace agent
} // namespace spiration
