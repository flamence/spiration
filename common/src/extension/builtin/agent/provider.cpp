/**
 * @file provider.cpp
 * @brief 对话 provider 实现。
 * @author clk
 */

#include <extension/builtin/agent/provider.h>
#include <utils/console.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <utility>

namespace spiration {
namespace agent {

namespace {

/// @brief 将思考等级映射为 OpenAI 的 reasoning_effort 值。
const char* openai_reasoning_effort(reasoning_level level) {
    switch (level) {
        case reasoning_level::none:     return nullptr;
        case reasoning_level::standard: return "medium";
        case reasoning_level::deep:     return "high";
    }
    return nullptr;
}

} // namespace

namespace {

class openai_provider : public provider {
public:
    std::string name() const override { return "openai"; }

    std::string chat_url(const std::string& endpoint) const override {
        return endpoint + "/chat/completions";
    }

    std::vector<std::pair<std::string, std::string>>
    request_headers(const std::string& api_key) const override {
        std::vector<std::pair<std::string, std::string>> headers;
        if (!api_key.empty())
            headers.push_back({"Authorization", "Bearer " + api_key});
        return headers;
    }

    std::string build_request_body(const provider_request& req) const override {
        nlohmann::json body;
        body["model"]       = req.model;
        if (req.max_tokens > 0) body["max_tokens"] = req.max_tokens;
        body["temperature"] = req.temperature;
        body["stream"]      = req.stream;

        if (const char* effort = openai_reasoning_effort(req.reasoning))
            body["reasoning_effort"] = effort;

        nlohmann::json messages = nlohmann::json::array();
        for (const auto& m : req.messages) {
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

        if (!req.tools.empty()) {
            nlohmann::json tools = nlohmann::json::array();
            for (const auto& def : req.tools) {
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

    provider_response parse_response(const std::string& body) const override {
        provider_response resp;

        nlohmann::json j = nlohmann::json::parse(body);
        if (j.contains("usage") && j["usage"].is_object()) {
            resp.prompt_tokens     = j["usage"].value("prompt_tokens", 0);
            resp.completion_tokens = j["usage"].value("completion_tokens", 0);
        }
        if (!j.contains("choices") || j["choices"].empty())
            return resp;

        auto& choice = j["choices"][0];
        resp.finish_reason = choice.value("finish_reason", "");

        if (!choice.contains("message"))
            return resp;

        auto& msg = choice["message"];
        resp.content = msg.value("content", "");
        resp.reasoning_content = msg.value("reasoning_content", "");

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

    void handle_stream_line(const std::string& line, stream_context& ctx) const override {
        if (line.rfind("data:", 0) != 0) return;

        std::string payload = line.substr(5);
        if (!payload.empty() && payload.front() == ' ') payload.erase(0, 1);
        if (payload == "[DONE]") return;

        nlohmann::json j;
        try { j = nlohmann::json::parse(payload); } catch (...) { return; }

        if (j.contains("usage") && j["usage"].is_object()) {
            ctx.prompt_tokens     = j["usage"].value("prompt_tokens", ctx.prompt_tokens);
            ctx.completion_tokens = j["usage"].value("completion_tokens", ctx.completion_tokens);
        }
        if (!j.contains("choices") || j["choices"].empty()) return;

        auto& choice = j["choices"][0];
        if (choice.contains("finish_reason") && choice["finish_reason"].is_string())
            ctx.finish_reason = choice["finish_reason"].get<std::string>();
        if (!choice.contains("delta")) return;

        auto& delta = choice["delta"];
        if (delta.contains("content") && delta["content"].is_string()) {
            std::string piece = delta["content"].get<std::string>();
            ctx.content += piece;
            if (ctx.on_delta) ctx.on_delta(piece);
        }
        if (delta.contains("reasoning_content") && delta["reasoning_content"].is_string()) {
            std::string piece = delta["reasoning_content"].get<std::string>();
            ctx.reasoning += piece;
            if (ctx.on_reasoning_delta) ctx.on_reasoning_delta(piece);
        }
        if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
            for (auto& t : delta["tool_calls"]) {
                int idx = t.value("index", 0);
                tool_call& tc = ctx.tc_map[idx];
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
};

} // namespace

namespace {

/// @brief 将 tool_call 的 arguments 解析为 JSON 对象。
nlohmann::json parse_tool_input(const std::string& arguments) {
    if (arguments.empty()) return nlohmann::json::object();
    try {
        nlohmann::json j = nlohmann::json::parse(arguments);
        if (j.is_object()) return j;
        return nlohmann::json::object();
    } catch (...) {
        return nlohmann::json::object();
    }
}

class anthropic_provider : public provider {
public:
    std::string name() const override { return "anthropic"; }

    std::string chat_url(const std::string& endpoint) const override {
        return endpoint + "/v1/messages";
    }

    std::vector<std::pair<std::string, std::string>>
    request_headers(const std::string& api_key) const override {
        std::vector<std::pair<std::string, std::string>> headers;
        if (!api_key.empty())
            headers.push_back({"x-api-key", api_key});
        headers.push_back({"anthropic-version", "2023-06-01"});
        return headers;
    }

    std::string build_request_body(const provider_request& req) const override {
        nlohmann::json body;
        body["model"]       = req.model;
        // max_tokens <= 0 表示不限制输出；Anthropic API 要求该字段，此时传较大默认值
        long out_tokens = req.max_tokens > 0 ? static_cast<long>(req.max_tokens) : 32768L;
        body["max_tokens"] = out_tokens;
        body["temperature"] = req.temperature;
        body["stream"]      = req.stream;

        // 深度思考：启用 thinking 块（预算取有效输出 token 的 3/4）
        if (req.reasoning == reasoning_level::deep) {
            long budget = std::max(1024L, out_tokens * 3L / 4L);
            body["thinking"] = {{"type", "enabled"}, {"budget_tokens", budget}};
        }

        std::string system;
        nlohmann::json messages = nlohmann::json::array();

        for (const auto& m : req.messages) {
            if (m.role == "system") {
                if (!system.empty()) system += "\n";
                system += m.content;
                continue;
            }
            if (m.role == "tool") {
                // 工具结果以 user 消息 + tool_result 内容块发送
                nlohmann::json block;
                block["type"]         = "tool_result";
                block["tool_use_id"]  = m.tool_call_id;
                block["content"]      = m.content;
                nlohmann::json msg;
                msg["role"]    = "user";
                msg["content"] = nlohmann::json::array({std::move(block)});
                messages.push_back(std::move(msg));
                continue;
            }
            if (m.role == "assistant" && !m.tool_calls.empty()) {
                nlohmann::json blocks = nlohmann::json::array();
                if (!m.content.empty())
                    blocks.push_back({{"type", "text"}, {"text", m.content}});
                for (const auto& tc : m.tool_calls) {
                    nlohmann::json block;
                    block["type"]  = "tool_use";
                    block["id"]    = tc.id;
                    block["name"]  = tc.function_name;
                    block["input"] = parse_tool_input(tc.arguments);
                    blocks.push_back(std::move(block));
                }
                nlohmann::json msg;
                msg["role"]    = "assistant";
                msg["content"] = std::move(blocks);
                messages.push_back(std::move(msg));
                continue;
            }
            nlohmann::json msg;
            msg["role"]    = m.role;
            msg["content"] = m.content;
            messages.push_back(std::move(msg));
        }

        if (!system.empty())
            body["system"] = system;
        body["messages"] = std::move(messages);

        if (!req.tools.empty()) {
            nlohmann::json tools = nlohmann::json::array();
            for (const auto& def : req.tools) {
                nlohmann::json tool;
                tool["name"]        = def.function_name;
                tool["description"] = def.description;
                tool["input_schema"] = nlohmann::json::parse(def.parameters_json);
                tools.push_back(std::move(tool));
            }
            body["tools"] = std::move(tools);
        }

        return body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    }

    provider_response parse_response(const std::string& body) const override {
        provider_response resp;

        nlohmann::json j = nlohmann::json::parse(body);
        resp.finish_reason = j.value("stop_reason", "");

        if (j.contains("usage") && j["usage"].is_object()) {
            resp.prompt_tokens     = j["usage"].value("input_tokens", 0);
            resp.completion_tokens = j["usage"].value("output_tokens", 0);
        }

        if (!j.contains("content") || !j["content"].is_array())
            return resp;

        for (auto& block : j["content"]) {
            std::string type = block.value("type", "");
            if (type == "text") {
                resp.content += block.value("text", "");
            } else if (type == "tool_use") {
                tool_call tc;
                tc.id            = block.value("id", "");
                tc.function_name = block.value("name", "");
                if (block.contains("input"))
                    tc.arguments = block["input"].dump(-1, ' ', false,
                                                       nlohmann::json::error_handler_t::replace);
                resp.tool_calls.push_back(std::move(tc));
            } else if (type == "thinking") {
                resp.reasoning_content += block.value("thinking", "");
            }
        }

        return resp;
    }

    void handle_stream_line(const std::string& line, stream_context& ctx) const override {
        if (line.rfind("event:", 0) == 0) {
            // 事件名本身无需处理，只消费该行避免误当 data 解析
            return;
        }
        if (line.rfind("data:", 0) != 0) return;

        std::string payload = line.substr(5);
        if (!payload.empty() && payload.front() == ' ') payload.erase(0, 1);

        nlohmann::json j;
        try { j = nlohmann::json::parse(payload); } catch (...) { return; }

        std::string type = j.value("type", "");
        if (type == "message_start" && j.contains("message")) {
            if (j["message"].contains("stop_reason") && j["message"]["stop_reason"].is_string())
                ctx.finish_reason = j["message"]["stop_reason"].get<std::string>();
            if (j["message"].contains("usage") && j["message"]["usage"].is_object())
                ctx.prompt_tokens = j["message"]["usage"].value("input_tokens", ctx.prompt_tokens);
            return;
        }
        if (type == "content_block_start" && j.contains("content_block")) {
            int idx = j.value("index", 0);
            auto& block = j["content_block"];
            tool_call& tc = ctx.tc_map[idx];
            if (block.contains("id"))   tc.id = block.value("id", "");
            if (block.contains("name")) tc.function_name = block.value("name", "");
            return;
        }
        if (type == "content_block_delta" && j.contains("delta")) {
            int idx = j.value("index", 0);
            auto& delta = j["delta"];
            std::string dt = delta.value("type", "");
            if (dt == "text_delta") {
                std::string piece = delta.value("text", "");
                ctx.content += piece;
                if (ctx.on_delta) ctx.on_delta(piece);
            } else if (dt == "input_json_delta") {
                ctx.tc_map[idx].arguments += delta.value("partial_json", "");
            } else if (dt == "thinking_delta") {
                std::string piece = delta.value("thinking", "");
                ctx.reasoning += piece;
                if (ctx.on_reasoning_delta) ctx.on_reasoning_delta(piece);
            }
            return;
        }
        if (type == "message_delta" && j.contains("delta")) {
            if (j["delta"].contains("stop_reason") && j["delta"]["stop_reason"].is_string())
                ctx.finish_reason = j["delta"]["stop_reason"].get<std::string>();
            if (j["delta"].contains("usage") && j["delta"]["usage"].is_object())
                ctx.completion_tokens = j["delta"]["usage"].value("output_tokens", ctx.completion_tokens);
            return;
        }
    }
};

} // namespace

std::unique_ptr<provider> create_provider(const std::string& name) {
    if (name == "anthropic") return std::make_unique<anthropic_provider>();
    return std::make_unique<openai_provider>();
}

} // namespace agent
} // namespace spiration
