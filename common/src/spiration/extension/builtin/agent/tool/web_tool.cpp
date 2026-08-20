/**
 * @file web_tool.cpp
 * @brief 互联网工具集实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/builtin/agent/tool/web_tool.h>
#include <spiration/net/http.h>

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdio>
#include <fmt/format.h>
#include <string>
#include <utility>
#include <vector>

namespace spiration {
namespace agent {

namespace {

std::string urlencode(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

} // namespace

std::string fetch_tool::description() const {
    return "Fetch a URL and return the HTTP status and response body. "
           "Supports custom method (GET/POST/PUT/PATCH/DELETE/HEAD), request headers, "
           "query params (appended to the URL), a request body, and a timeout in seconds.";
}

nlohmann::json fetch_tool::parameters() const {
    return nlohmann::json::parse(R"({
    "type": "object",
    "properties": {
        "url": {
            "type": "string",
            "description": "The URL to fetch."
        },
        "method": {
            "type": "string",
            "description": "HTTP method. Default GET."
        },
        "headers": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "name": {"type": "string"},
                    "value": {"type": "string"}
                }
            },
            "description": "Request headers as {name, value} pairs."
        },
        "params": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "name": {"type": "string"},
                    "value": {"type": "string"}
                }
            },
            "description": "Query parameters appended to the URL."
        },
        "body": {
            "type": "string",
            "description": "Request body (used with POST/PUT/PATCH)."
        },
        "timeout": {
            "type": "integer",
            "description": "Timeout in seconds. Default 30."
        }
    },
    "required": ["url"]
})");
}

std::string fetch_tool::execute(const std::string& args_json) {
    std::string url, method = "GET", body;
    long timeout = 30;
    std::vector<std::pair<std::string, std::string>> headers, params;
    try {
        nlohmann::json j = nlohmann::json::parse(args_json);
        url = j.value("url", "");
        method = j.value("method", "GET");
        body = j.value("body", "");
        timeout = j.value("timeout", static_cast<long>(30));
        if (j.contains("headers") && j["headers"].is_array()) {
            for (auto& h : j["headers"]) {
                if (!h.is_object()) continue;
                headers.push_back({h.value("name", ""), h.value("value", "")});
            }
        }
        if (j.contains("params") && j["params"].is_array()) {
            for (auto& p : j["params"]) {
                if (!p.is_object()) continue;
                params.push_back({p.value("name", ""), p.value("value", "")});
            }
        }
    } catch (const std::exception& e) {
        return "[error] invalid arguments: " + std::string(e.what());
    }
    if (url.empty()) return "[error] missing 'url'";
    if (timeout <= 0) timeout = 30;

    std::string full_url = url;
    if (!params.empty()) {
        std::string qs;
        for (size_t i = 0; i < params.size(); ++i) {
            if (i) qs += "&";
            qs += urlencode(params[i].first) + "=" + urlencode(params[i].second);
        }
        full_url += (full_url.find('?') == std::string::npos) ? "?" : "&";
        full_url += qs;
    }

    net::http_request req;
    req.method = method;
    req.url = full_url;
    req.body = body;
    req.timeout_seconds = timeout;
    req.user_agent = "spiration/1.0";
    for (const auto& h : headers) {
        if (!h.first.empty()) req.headers.push_back(h);
    }

    net::http_response resp = net::http().request(req);

    std::string out = "[status " + std::to_string(resp.status) + "]";
    if (!resp.error.empty()) {
        out += " [error] " + resp.error;
    }
    if (!resp.body.empty()) {
        out += "\n" + resp.body;
    }
    application::instance()->logger()->log({logging::level::info, fmt::format("fetch {} -> status {}, {} bytes", full_url, resp.status, resp.body.size()), "extension/agent/web"});
    return out;
}

} // namespace agent
} // namespace spiration
