/**
 * @file chat.cpp
 * @brief OpenAI 兼容对话客户端实现。
 * @author clk
 */

#include <extension/builtin/agent/chat.h>
#include <utils/console.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <exception>
#include <map>
#include <sstream>
#include <cstdio>

#ifndef OHOS_PLATFORM
#include <curl/curl.h>
#endif

namespace spiration {
namespace agent {

chat_client::chat_client(const config& cfg) : cfg_(cfg) {}

chat_client::~chat_client() = default;

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

std::string chat_client::build_request_body() const {
    nlohmann::json body;
    body["model"]      = cfg_.model;
    body["max_tokens"] = cfg_.max_tokens;
    body["temperature"] = cfg_.temperature;
    body["stream"]     = cfg_.stream;

    nlohmann::json messages = nlohmann::json::array();
    for (const auto& m : history_) {
        nlohmann::json msg;
        msg["role"] = m.role;

        if (m.role == "assistant" && !m.tool_calls.empty()) {
            msg["content"] = nullptr;
            nlohmann::json tcs = nlohmann::json::array();
            for (const auto& tc : m.tool_calls) {
                nlohmann::json j;
                j["id"]   = tc.id;
                j["type"] = "function";
                j["function"]["name"]      = tc.function_name;
                j["function"]["arguments"] = tc.arguments;
                tcs.push_back(std::move(j));
            }
            msg["tool_calls"] = std::move(tcs);
        } else {
            msg["content"] = m.content;
        }

        if (!m.tool_call_id.empty())
            msg["tool_call_id"] = m.tool_call_id;
        if (!m.name.empty())
            msg["name"] = m.name;
        messages.push_back(std::move(msg));
    }
    body["messages"] = std::move(messages);

    if (!tools_.empty()) {
        nlohmann::json tools = nlohmann::json::array();
        for (const auto* t : tools_) {
            tool_definition def = t->to_definition();
            nlohmann::json tool;
            tool["type"] = "function";
            tool["function"]["name"]        = def.function_name;
            tool["function"]["description"] = def.description;
            tool["function"]["parameters"]  = nlohmann::json::parse(def.parameters_json);
            tools.push_back(std::move(tool));
        }
        body["tools"]       = std::move(tools);
        body["tool_choice"] = "auto";
    }

    return body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

chat_response chat_client::parse_response(const std::string& body) const {
    chat_response resp;

    nlohmann::json j = nlohmann::json::parse(body);
    if (!j.contains("choices") || j["choices"].empty())
        return resp;

    auto& choice = j["choices"][0];
    resp.finish_reason = choice.value("finish_reason", "");

    if (!choice.contains("message"))
        return resp;

    auto& msg = choice["message"];
    resp.content = msg.value("content", "");

    if (msg.contains("tool_calls")) {
        for (auto& tc : msg["tool_calls"]) {
            tool_call tc_out;
            tc_out.id = tc.value("id", "");
            if (tc.contains("function")) {
                tc_out.function_name = tc["function"].value("name", "");
                tc_out.arguments     = tc["function"].value("arguments", "");
            }
            resp.tool_calls.push_back(std::move(tc_out));
        }
    }

    return resp;
}

struct stream_context {
    std::string line_buf;
    std::string content;
    std::string finish_reason;
    std::map<int, tool_call> tc_map;
    std::function<void(const std::string&)> on_delta;
};

static size_t stream_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* ctx = static_cast<stream_context*>(userp);
    size_t total = size * nmemb;
    if (!ctx) return total;

    ctx->line_buf.append(static_cast<char*>(contents), total);
    size_t pos;
    while ((pos = ctx->line_buf.find('\n')) != std::string::npos) {
        std::string line = ctx->line_buf.substr(0, pos);
        ctx->line_buf.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("data:", 0) != 0) continue;

        std::string payload = line.substr(5);
        if (!payload.empty() && payload.front() == ' ') payload.erase(0, 1);
        if (payload == "[DONE]") continue;

        nlohmann::json j;
        try { j = nlohmann::json::parse(payload); } catch (...) { continue; }
        if (!j.contains("choices") || j["choices"].empty()) continue;

        auto& choice = j["choices"][0];
        if (choice.contains("finish_reason") && choice["finish_reason"].is_string())
            ctx->finish_reason = choice["finish_reason"].get<std::string>();
        if (!choice.contains("delta")) continue;

        auto& delta = choice["delta"];
        if (delta.contains("content") && delta["content"].is_string()) {
            std::string piece = delta["content"].get<std::string>();
            ctx->content += piece;
            if (ctx->on_delta) ctx->on_delta(piece);
        }
        if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
            for (auto& t : delta["tool_calls"]) {
                int idx = t.value("index", 0);
                tool_call& tc = ctx->tc_map[idx];
                if (t.contains("id")) tc.id = t.value("id", "");
                if (t.contains("function")) {
                    if (t["function"].contains("name"))
                        tc.function_name = t["function"].value("name", "");
                    if (t["function"].contains("arguments"))
                        tc.arguments += t["function"].value("arguments", "");
                }
            }
        }
    }
    return total;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

std::string chat_client::http_post(const std::string& url, const std::string& body,
                                    const std::string& api_key) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        console::error("extension/agent", "curl_easy_init failed");
        return {};
    }

    std::string response;
    struct curl_slist* headers = nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "spiration/1.0");

    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!api_key.empty()) {
        std::string auth = "Authorization: Bearer " + api_key;
        headers = curl_slist_append(headers, auth.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        console::error("extension/agent", "curl request failed: %s", curl_easy_strerror(res));
        response.clear();
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

chat_response chat_client::send() {
    return send_stream(nullptr);
}

void chat_client::append_assistant(const chat_response& resp) {
    if (!resp.content.empty() || !resp.tool_calls.empty()) {
        chat_message assistant;
        assistant.role       = "assistant";
        assistant.content    = resp.content;
        assistant.tool_calls = resp.tool_calls;
        history_.push_back(assistant);
    }
}

chat_response chat_client::send_stream(const std::function<void(const std::string&)>& on_delta) {
    std::string url = cfg_.endpoint + "/chat/completions";
    std::string body;
    try {
        body = build_request_body();
    } catch (const std::exception& e) {
        console::error("extension/agent", "build request body failed: %s", e.what());
        return {};
    }

    console::info("extension/agent", "POST %s (stream=%d)", url.c_str(), cfg_.stream ? 1 : 0);

    if (!cfg_.stream) {
        std::string response = http_post(url, body, cfg_.api_key);
        if (response.empty()) {
            console::warning("extension/agent", "empty response");
            return {};
        }
        chat_response resp;
        try {
            resp = parse_response(response);
        } catch (const std::exception& e) {
            console::error("extension/agent", "parse response failed: %s", e.what());
            return {};
        }
        if (on_delta && !resp.content.empty())
            on_delta(resp.content);
        append_assistant(resp);
        return resp;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        console::error("extension/agent", "curl_easy_init failed");
        return {};
    }

    stream_context ctx;
    ctx.on_delta = on_delta;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "spiration/1.0");

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!cfg_.api_key.empty()) {
        std::string auth = "Authorization: Bearer " + cfg_.api_key;
        headers = curl_slist_append(headers, auth.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        console::error("extension/agent", "curl stream request failed: %s", curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    chat_response resp;
    resp.content       = ctx.content;
    resp.finish_reason = ctx.finish_reason;
    for (auto& [idx, tc] : ctx.tc_map)
        resp.tool_calls.push_back(std::move(tc));

    append_assistant(resp);
    return resp;
}

std::string chat_client::execute_tool(const tool_call& tc) {
    for (auto* t : tools_) {
        if (t && t->name() == tc.function_name) {
            try {
                console::info("extension/agent", "executing tool: %s", tc.function_name.c_str());
                return t->execute(tc.arguments);
            } catch (const std::exception& e) {
                console::error("extension/agent", "tool '%s' threw: %s",
                               tc.function_name.c_str(), e.what());
                return "[error] tool execution failed: " + std::string(e.what());
            }
        }
    }
    console::warning("extension/agent", "tool not found: %s", tc.function_name.c_str());
    return "[error] tool not found: " + tc.function_name;
}

chat_response chat_client::run(int max_rounds, const chat_events& events) {
    chat_response last;
    std::vector<tool_execution> all_executions;

    for (int round = 0; round < max_rounds; ++round) {
        last = send_stream(events.on_delta);

        if (last.tool_calls.empty())
            break;

        for (auto& tc : last.tool_calls) {
            if (events.on_tool_call) events.on_tool_call(tc);

            std::string result = execute_tool(tc);

            tool_execution ex;
            ex.function_name = tc.function_name;
            ex.arguments     = tc.arguments;
            ex.result        = result;
            all_executions.push_back(ex);

            if (events.on_tool_result) events.on_tool_result(ex);

            chat_message tool_msg;
            tool_msg.role         = "tool";
            tool_msg.content      = result;
            tool_msg.tool_call_id = tc.id;
            tool_msg.name         = tc.function_name;
            history_.push_back(std::move(tool_msg));
        }
    }

    last.executions = std::move(all_executions);
    return last;
}

} // namespace agent
} // namespace spiration
