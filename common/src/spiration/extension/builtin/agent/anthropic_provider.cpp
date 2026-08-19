/**
 * @file anthropic_provider.cpp
 * @brief Anthropic Messages provider 实现。
 * @author clk
 */

#include <spiration/extension/builtin/agent/anthropic_provider.h>

#include <nlohmann/json.hpp>

#include <algorithm>

namespace spiration {
namespace agent {

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

} // namespace

void anthropic_provider::build_reasoning(web_context& ctx, reasoning_level level) const {
    // Anthropic API 要求 max_tokens 字段；max_tokens <= 0 表示不限制输出，此时传较大默认值。
    long out_tokens = 32768L;
    if (ctx.payload.contains("max_tokens") && ctx.payload["max_tokens"].is_number()) {
        out_tokens = ctx.payload["max_tokens"].get<long>();
    } else {
        ctx.payload["max_tokens"] = out_tokens;
    }

    // 思考：按挡位决定 thinking 块预算。
    // none 时不发送 thinking（Anthropic 默认不思考）；其余挡位按比例分配 budget_tokens。
    if (level != reasoning_level::none) {
        double ratio = 0.5;
        switch (level) {
            case reasoning_level::low:    ratio = 0.25; break;
            case reasoning_level::medium: ratio = 0.50; break;
            case reasoning_level::high:   ratio = 0.75; break;
            case reasoning_level::xhigh:  ratio = 0.85; break;
            case reasoning_level::max:    ratio = 0.95; break;
            default: break;
        }
        long budget = std::max(1024L, static_cast<long>(static_cast<double>(out_tokens) * ratio));
        if (budget > out_tokens) budget = out_tokens;
        ctx.payload["thinking"] = {{"type", "enabled"}, {"budget_tokens", budget}};
    }
}

void anthropic_provider::build_tools(web_context& ctx,
                                     const std::vector<tool_definition>& tools) const {
    if (tools.empty()) return;

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& def : tools) {
        nlohmann::json tool;
        tool["name"]         = def.function_name;
        tool["description"]  = def.description;
        tool["input_schema"] = nlohmann::json::parse(def.parameters_json);
        arr.push_back(std::move(tool));
    }
    ctx.payload["tools"] = std::move(arr);
}

void anthropic_provider::build_messages(web_context& ctx) const {
    const auto& req = ctx.request();

    std::string system;
    nlohmann::json messages = nlohmann::json::array();

    for (const auto& m : req.messages) {
        if (m.role == "system") {
            if (!system.empty()) system += "\n";
            system += m.content;
            continue;
        }
        if (m.role == "tool") {
            // 工具结果以 user 消息 + tool_result 内容块发送。
            nlohmann::json block;
            block["type"]        = "tool_result";
            block["tool_use_id"] = m.tool_call_id;
            block["content"]     = m.content;
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
        ctx.payload["system"] = system;
    ctx.payload["messages"] = std::move(messages);
}

provider_response anthropic_provider::parse_response(const std::string& body) const {
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

void anthropic_provider::handle_stream_line(const std::string& line, web_context& ctx) const {
    if (line.rfind("event:", 0) == 0) {
        // 事件名本身无需处理，只消费该行避免误当 data 解析。
        return;
    }

    std::string payload;
    if (!sse_data_payload(line, payload)) return;

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

} // namespace agent
} // namespace spiration
