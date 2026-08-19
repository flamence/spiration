/**
 * @file web_provider.cpp
 * @brief 基于 HTTP/SSE 的对话补全 provider 基类实现。
 * @author clk
 */

#include <spiration/extension/builtin/agent/web_provider.h>

#include <spiration/application.h>
#include <spiration/net/http.h>

#include <fmt/format.h>

namespace spiration {
namespace agent {

std::unique_ptr<provider_context> web_provider::build_context(const provider_request& req) const {
    auto ctx = std::make_unique<web_context>(req);
    ctx->endpoint = req.endpoint;
    ctx->api_key = req.api_key;
    ctx->timeout_seconds = req.timeout_seconds;
    ctx->url = req.endpoint + api_path();

    if (!req.api_key.empty()) {
        auto auth = authorization_header(req.api_key);
        if (!auth.first.empty()) ctx->headers.push_back(std::move(auth));
    }
    append_headers(ctx->headers);

    // 请求体基础字段。
    ctx->payload["model"]       = req.model;
    if (req.max_tokens > 0) ctx->payload["max_tokens"] = req.max_tokens;
    ctx->payload["temperature"] = req.temperature;
    ctx->payload["stream"]      = req.stream;

    // 结构化处理：思考过程、工具调用、消息。
    build_reasoning(*ctx, req.reasoning);
    build_tools(*ctx, req.tools);
    build_messages(*ctx);

    ctx->body = dump_json(ctx->payload);
    return ctx;
}

std::string web_provider::http_post(const web_context& ctx) const {
    net::http_request req;
    req.method = "POST";
    req.url = ctx.url;
    req.body = ctx.body;
    req.timeout_seconds = ctx.timeout_seconds;
    req.user_agent = "spiration/1.0";
    req.headers = ctx.headers;
    req.headers.push_back({"Content-Type", "application/json"});

    net::http_response resp = net::http().request(req);
    if (!resp.error.empty()) {
        application::instance()->logger()->log({logging::level::error, fmt::format("http request failed: {}", resp.error), "extension/agent"});
        return {};
    }
    return resp.body;
}

provider_response web_provider::execute(provider_context& base_ctx) const {
    web_context& ctx = static_cast<web_context&>(base_ctx);

    application::instance()->logger()->log({logging::level::info, fmt::format("POST {} (provider={}, stream={})", ctx.url, name(), ctx.request().stream ? 1 : 0), "extension/agent"});

    if (!ctx.request().stream) {
        std::string response = http_post(ctx);
        if (response.empty()) {
            application::instance()->logger()->log({logging::level::warning, "empty response", "extension/agent"});
            return {};
        }
        try {
            return parse_response(response);
        } catch (const std::exception& e) {
            application::instance()->logger()->log({logging::level::error, fmt::format("parse response failed: {}", e.what()), "extension/agent"});
            return {};
        }
    }

    // 流式：SSE 分块 → handle_stream_line 累积。
    net::http_request http_req;
    http_req.method = "POST";
    http_req.url = ctx.url;
    http_req.body = ctx.body;
    http_req.timeout_seconds = ctx.timeout_seconds;
    http_req.user_agent = "spiration/1.0";
    http_req.headers = ctx.headers;
    http_req.headers.push_back({"Content-Type", "application/json"});

    net::http_response http_resp = net::http().request_stream(
        http_req,
        [this, &ctx](const char* data, size_t size) { process_stream_chunk(ctx, data, size); },
        ctx.should_stop);

    if (!http_resp.error.empty()) {
        if (!(ctx.should_stop && ctx.should_stop())) {
            application::instance()->logger()->log({logging::level::error, fmt::format("http stream request failed: {}", http_resp.error), "extension/agent"});
        }
    }

    return to_response(ctx);
}

void web_provider::process_stream_chunk(web_context& ctx, const char* data, size_t size) const {
    if (!data || size == 0) return;
    if (ctx.should_stop && ctx.should_stop()) return;

    ctx.line_buf.append(data, size);
    size_t pos;
    while ((pos = ctx.line_buf.find('\n')) != std::string::npos) {
        std::string line = ctx.line_buf.substr(0, pos);
        ctx.line_buf.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        handle_stream_line(line, ctx);
    }
}

provider_response web_provider::to_response(const web_context& ctx) {
    provider_response resp;
    resp.content           = ctx.content;
    resp.reasoning_content = ctx.reasoning;
    resp.finish_reason     = ctx.finish_reason;
    resp.prompt_tokens     = ctx.prompt_tokens;
    resp.completion_tokens = ctx.completion_tokens;
    for (const auto& tc : ctx.tc_map) {
        resp.tool_calls.push_back(tc.second);
    }
    return resp;
}

bool web_provider::sse_data_payload(const std::string& line, std::string& payload) {
    if (line.rfind("data:", 0) != 0) return false;
    payload = line.substr(5);
    if (!payload.empty() && payload.front() == ' ') payload.erase(0, 1);
    return true;
}

std::string web_provider::dump_json(const nlohmann::json& j) {
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

} // namespace agent
} // namespace spiration
