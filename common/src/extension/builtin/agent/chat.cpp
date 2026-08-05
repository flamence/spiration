/**
 * @file chat.cpp
 * @brief 对话客户端实现。
 * @author clk
 */

#include <extension/builtin/agent/chat.h>
#include <extension/builtin/agent/registry.h>
#include <utils/console.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <exception>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include <utility>

#include <curl/curl.h>

namespace spiration {
namespace agent {

chat_client::chat_client(const config& cfg) : cfg_(cfg) {
    provider_ = agent_registry::instance().create_provider(cfg_.provider);
}

void chat_client::configure(const config& c) {
    cfg_ = c;
    provider_ = agent_registry::instance().create_provider(cfg_.provider);
    console::info("extension/agent", "configured: provider=%s model=%s",
                  cfg_.provider.c_str(), cfg_.model.c_str());
}

chat_client::~chat_client() = default;

std::string chat_client::provider_name() const {
    return provider_ ? provider_->name() : cfg_.provider;
}

void chat_client::set_system_prompt(const std::string& prompt) {
    history_.erase(
        std::remove_if(history_.begin(), history_.end(),
                       [](const chat_message& m) { return m.role == "system"; }),
        history_.end());
    if (!prompt.empty()) {
        history_.insert(history_.begin(), {"system", prompt, "", "", {}});
    }
}

void chat_client::add_message(const chat_message& msg) {
    history_.push_back(msg);
}

void chat_client::clear_history() {
    history_.erase(
        std::remove_if(history_.begin(), history_.end(),
                       [](const chat_message& m) { return m.role != "system"; }),
        history_.end());
}

void chat_client::register_tool(tool* t) {
    if (t) tools_.push_back(t);
}

void chat_client::clear_tools() {
    tools_.clear();
}

static size_t stream_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* ctx = static_cast<stream_context*>(userp);
    size_t total = size * nmemb;
    if (!ctx) return total;

    if (ctx->should_stop && ctx->should_stop()) return 0;

    ctx->line_buf.append(static_cast<char*>(contents), total);
    size_t pos;
    while ((pos = ctx->line_buf.find('\n')) != std::string::npos) {
        std::string line = ctx->line_buf.substr(0, pos);
        ctx->line_buf.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (ctx->provider) ctx->provider->handle_stream_line(line, *ctx);
    }
    return total;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

std::string chat_client::http_post(const std::string& url, const std::string& body,
                                   const std::vector<std::pair<std::string, std::string>>& headers) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        console::error("extension/agent", "curl_easy_init failed");
        return {};
    }

    std::string response;
    struct curl_slist* hdr_list = nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, cfg_.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "spiration/1.0");

    hdr_list = curl_slist_append(hdr_list, "Content-Type: application/json");
    for (const auto& h : headers) {
        if (h.first.empty()) continue;
        hdr_list = curl_slist_append(hdr_list, (h.first + ": " + h.second).c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        console::error("extension/agent", "curl request failed: %s", curl_easy_strerror(res));
        response.clear();
    }

    curl_slist_free_all(hdr_list);
    curl_easy_cleanup(curl);
    return response;
}

chat_response chat_client::send() {
    return send_stream(chat_events{});
}

void chat_client::append_assistant(const chat_response& resp) {
    if (!resp.content.empty() || !resp.tool_calls.empty() ||
        !resp.reasoning_content.empty()) {
        chat_message assistant;
        assistant.role            = "assistant";
        assistant.content         = resp.content;
        assistant.reasoning_content = resp.reasoning_content;
        assistant.tool_calls      = resp.tool_calls;
        history_.push_back(assistant);
    }
}

void chat_client::sanitize_history() {
    history_.erase(
        std::remove_if(history_.begin(), history_.end(),
                       [](const chat_message& m) {
                           if (m.role == "system") return false;
                           if (m.role == "assistant")
                               return m.content.empty() && m.tool_calls.empty() &&
                                      m.tool_call_id.empty() &&
                                      m.reasoning_content.empty();
                           return m.content.empty();
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
        if (m.content.empty()) {
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
                           return m.tool_call_id.empty() ||
                                  valid_ids.count(m.tool_call_id) == 0;
                       }),
        history_.end());
}

chat_response chat_client::send_stream(const chat_events& events) {
    if (!provider_) {
        console::error("extension/agent", "no provider configured");
        return {};
    }
    sanitize_history();

    provider_request req;
    req.model       = cfg_.model;
    req.max_tokens  = cfg_.max_tokens;
    req.temperature = cfg_.temperature;
    req.stream      = cfg_.stream;
    req.reasoning   = cfg_.reasoning;
    req.messages    = history_;
    for (const auto* t : tools_) {
        if (t) req.tools.push_back(t->to_definition());
    }

    std::string url = provider_->chat_url(cfg_.endpoint);
    auto headers = provider_->request_headers(cfg_.api_key);
    std::string body;
    try {
        body = provider_->build_request_body(req);
    } catch (const std::exception& e) {
        console::error("extension/agent", "build request body failed: %s", e.what());
        return {};
    }

    console::info("extension/agent", "POST %s (provider=%s, stream=%d)",
                  url.c_str(), provider_->name().c_str(), cfg_.stream ? 1 : 0);

    if (!cfg_.stream) {
        std::string response = http_post(url, body, headers);
        if (response.empty()) {
            console::warning("extension/agent", "empty response");
            return {};
        }
        chat_response resp;
        try {
            provider_response pr = provider_->parse_response(response);
            resp.content           = pr.content;
            resp.reasoning_content = pr.reasoning_content;
            resp.tool_calls        = std::move(pr.tool_calls);
            resp.finish_reason     = pr.finish_reason;
            total_input_  += pr.prompt_tokens;
            total_output_ += pr.completion_tokens;
            if (on_tokens_updated) on_tokens_updated();
        } catch (const std::exception& e) {
            console::error("extension/agent", "parse response failed: %s", e.what());
            return {};
        }
        if (events.on_delta && !resp.content.empty())
            events.on_delta(resp.content);
        if (events.on_reasoning_delta && !resp.reasoning_content.empty())
            events.on_reasoning_delta(resp.reasoning_content);
        append_assistant(resp);
        return resp;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        console::error("extension/agent", "curl_easy_init failed");
        return {};
    }

    stream_context ctx;
    ctx.provider = provider_.get();
    ctx.on_delta = events.on_delta;
    ctx.on_reasoning_delta = events.on_reasoning_delta;
    ctx.should_stop = events.should_stop;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, cfg_.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "spiration/1.0");

    struct curl_slist* hdr_list = nullptr;
    hdr_list = curl_slist_append(hdr_list, "Content-Type: application/json");
    for (const auto& h : headers) {
        if (h.first.empty()) continue;
        hdr_list = curl_slist_append(hdr_list, (h.first + ": " + h.second).c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        if (!(events.should_stop && events.should_stop())) {
            console::error("extension/agent", "curl stream request failed: %s",
                           curl_easy_strerror(res));
        }
    }

    curl_slist_free_all(hdr_list);
    curl_easy_cleanup(curl);

    chat_response resp;
    resp.content           = ctx.content;
    resp.reasoning_content = ctx.reasoning;
    resp.finish_reason     = ctx.finish_reason;
    total_input_  += ctx.prompt_tokens;
    total_output_ += ctx.completion_tokens;
    if (on_tokens_updated) on_tokens_updated();
    for (auto& [idx, tc] : ctx.tc_map)
        resp.tool_calls.push_back(std::move(tc));

    append_assistant(resp);
    return resp;
}

tool* chat_client::find_tool(const std::string& name) const {
    for (auto* t : tools_) {
        if (t && t->name() == name) return t;
    }
    return nullptr;
}

bool chat_client::switch_provider(const std::string& name) {
    if (name.empty()) return true;
    if (provider_ && provider_->name() == name) return true;
    auto p = agent_registry::instance().create_provider(name);
    if (!p) {
        console::warning("extension/agent", "switch provider failed: %s", name.c_str());
        return false;
    }
    provider_ = std::move(p);
    console::info("extension/agent", "switched provider to %s", name.c_str());
    return true;
}

std::future<std::string> chat_client::launch_tool(const tool_call& tc,
                                                  const std::function<bool()>& should_stop) {
    tool* t = find_tool(tc.function_name);
    if (!t) {
        console::warning("extension/agent", "tool not found: %s", tc.function_name.c_str());
        std::promise<std::string> p;
        p.set_value("[error] tool not found: " + tc.function_name);
        return p.get_future();
    }

    if (!t->should_stop) t->should_stop = should_stop;
    console::info("extension/agent", "executing tool: %s", tc.function_name.c_str());

    auto task = std::make_shared<std::packaged_task<std::string()>>(
        [t, args = tc.arguments]() { return t->execute(args); });
    std::future<std::string> fut = task->get_future();
    std::thread([task]() { (*task)(); }).detach();
    return fut;
}

std::string chat_client::execute_tool(const tool_call& tc,
                                      const std::function<bool()>& should_stop) {
    tool* t = find_tool(tc.function_name);
    long timeout = t ? t->default_timeout_seconds() : 30;

    std::future<std::string> fut = launch_tool(tc, should_stop);
    if (timeout > 0 && fut.wait_for(std::chrono::seconds(timeout)) == std::future_status::timeout) {
        console::warning("extension/agent", "tool '%s' timed out after %lds",
                         tc.function_name.c_str(), timeout);
        return "[error] tool '" + tc.function_name + "' timed out after " +
               std::to_string(timeout) + "s";
    }
    try {
        return fut.get();
    } catch (const std::exception& e) {
        console::error("extension/agent", "tool '%s' threw: %s",
                       tc.function_name.c_str(), e.what());
        return "[error] tool execution failed: " + std::string(e.what());
    }
}

chat_response chat_client::run(int max_rounds, const chat_events& events) {
    chat_response last;
    std::vector<tool_execution> all_executions;

    constexpr size_t MAX_CONCURRENT = 4;

    for (int round = 0; round < max_rounds; ++round) {
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
            history_.push_back(std::move(tool_msg));
        }
    }

    last.executions = std::move(all_executions);
    return last;
}

} // namespace agent
} // namespace spiration
