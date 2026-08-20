/**
 * @file openai_provider.cpp
 * @brief OpenAI Chat Completions provider 实现。
 * @author clk
 */

#include <spiration/extension/builtin/agent/openai_provider.h>

#include <nlohmann/json.hpp>

namespace spiration {
namespace agent {

namespace {

/// @brief 将思考挡位映射为 OpenAI 的 reasoning_effort 值。
///        仅对启用思考的挡位返回非空；none 由调用方以 thinking disabled 处理。
const char* openai_reasoning_effort(reasoning_level level) {
    switch (level) {
        case reasoning_level::none:   return nullptr;
        case reasoning_level::low:    return "low";
        case reasoning_level::medium: return "medium";
        case reasoning_level::high:   return "high";
        case reasoning_level::xhigh:  return "xhigh";
        case reasoning_level::max:    return "max";
    }
    return nullptr;
}

} // namespace

void openai_provider::build_reasoning(web_context& ctx, reasoning_level level) const {
    if (const char* effort = openai_reasoning_effort(level)) {
        // OpenAI 思考模型：effort 控制思考强度；
        // 参考 python 客户端 extra_body={"thinking": {"type": "enabled"}}，
        // 显式声明 thinking 开启，避免默认行为不一致。
        ctx.payload["reasoning_effort"] = effort;
        ctx.payload["thinking"] = {{"type", "enabled"}};
    } else if (level == reasoning_level::none) {
        // “无”：显式关闭思考，而不是退回模型默认（否则部分模型仍会思考）。
        ctx.payload["thinking"] = {{"type", "disabled"}};
    }
}

void openai_provider::build_tools(web_context& ctx,
                                  const std::vector<tool_definition>& tools) const {
    if (tools.empty()) return;

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& def : tools) {
        nlohmann::json tool;
        tool["type"] = "function";
        tool["function"]["name"]        = def.function_name;
        tool["function"]["description"] = def.description;
        tool["function"]["parameters"]  = def.parameters;
        arr.push_back(std::move(tool));
    }
    ctx.payload["tools"]       = std::move(arr);
    ctx.payload["tool_choice"] = "auto";
}

void openai_provider::build_messages(web_context& ctx) const {
    const auto& req = ctx.request();
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

    ctx.payload["messages"] = std::move(messages);
}

provider_response openai_provider::parse_response(const std::string& body) const {
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

void openai_provider::handle_stream_line(const std::string& line, web_context& ctx) const {
    std::string payload;
    if (!sse_data_payload(line, payload)) return;
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

} // namespace agent
} // namespace spiration
