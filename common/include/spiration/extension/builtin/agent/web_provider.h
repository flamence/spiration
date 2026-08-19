/**
 * @file web_provider.h
 * @brief 基于 HTTP/SSE 的对话补全 provider 基类。
 * @author clk
 */

#pragma once

#include <spiration/extension/builtin/agent/provider.h>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace spiration {
namespace agent {

/**
 * @brief 基于 HTTP/SSE 的 provider 基类。
 *        在 provider 之上补充网络传输：
 *          - build_context()：组装端点/鉴权/请求体（通过协议钩子序列化思考、工具与消息）；
 *          - execute()：发送（或流式发送）HTTP 请求并解析响应；
 *        具体协议差异（思考、工具、消息、响应/流式解析）由子类实现协议钩子。
 */
class web_provider : public provider {
public:
    /// @brief HTTP/SSE 补全上下文。
    class web_context : public provider_context {
    public:
        explicit web_context(const provider_request& req) : provider_context(req) {}

        /// 传输参数。
        std::string endpoint;
        std::string api_key;
        long timeout_seconds = 120;

        /// 请求（由 build_context 组装）。
        std::string url;
        std::vector<std::pair<std::string, std::string>> headers;
        nlohmann::json payload;   ///< 构建中的请求体（协议钩子填充）。
        std::string body;         ///< 序列化后的请求体。

        /// 流式累积状态（execute 流式路径使用）。
        std::string line_buf;
        std::string content;
        std::string reasoning;
        std::string finish_reason;
        int prompt_tokens = 0;
        int completion_tokens = 0;
        std::map<int, tool_call> tc_map;
    };

    std::unique_ptr<provider_context> build_context(const provider_request& req) const override;
    provider_response execute(provider_context& ctx) const override;

protected:
    /// @brief 补全请求路径；url = endpoint + api_path()。
    virtual std::string api_path() const = 0;

    /// @brief 鉴权请求头；返回空 pair 表示不附加。
    virtual std::pair<std::string, std::string>
    authorization_header(const std::string& api_key) const = 0;

    /// @brief 追加额外请求头（如 Anthropic 的 anthropic-version）。
    virtual void append_headers(
        std::vector<std::pair<std::string, std::string>>& headers) const {
        (void)headers;
    }

    /// @brief 将思考挡位序列化进请求体（默认无操作）。
    virtual void build_reasoning(web_context& ctx, reasoning_level level) const {
        (void)ctx; (void)level;
    }

    /// @brief 将工具定义序列化进请求体（默认无操作）。
    virtual void build_tools(web_context& ctx,
                             const std::vector<tool_definition>& tools) const {
        (void)ctx; (void)tools;
    }

    /// @brief 将消息序列化进请求体。
    virtual void build_messages(web_context& ctx) const = 0;

    /// @brief 解析非流式响应。
    virtual provider_response parse_response(const std::string& body) const = 0;

    /// @brief 处理一条流式行。
    virtual void handle_stream_line(const std::string& line, web_context& ctx) const = 0;

    /// @brief 从 SSE 行提取 "data:" 负载；非 data 行返回 false。
    static bool sse_data_payload(const std::string& line, std::string& payload);

    /// @brief 序列化 JSON。
    static std::string dump_json(const nlohmann::json& j);

    /// @brief 将流式累积的 web_context 汇总为 provider_response。
    static provider_response to_response(const web_context& ctx);

    /// @brief 将收到的数据块按行切分并交给 handle_stream_line（SSE）。
    void process_stream_chunk(web_context& ctx, const char* data, size_t size) const;

private:
    /// @brief 发送非流式 POST，返回响应体。
    std::string http_post(const web_context& ctx) const;
};

} // namespace agent
} // namespace spiration
