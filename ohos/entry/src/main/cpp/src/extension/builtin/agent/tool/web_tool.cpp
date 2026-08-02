/**
 * @file web_tool.cpp
 * @brief OpenHarmony 平台 web 工具桩（无 libcurl，网络能力由 ArkTS 侧提供）。
 * @author clk
 */

#include <extension/builtin/agent/tool/web_tool.h>

#include <string>

namespace spiration {
namespace agent {

std::string fetch_tool::description() const {
    return "Fetch a URL and return the HTTP status and response body. "
           "Supports custom method (GET/POST/PUT/PATCH/DELETE/HEAD), request headers, "
           "query params (appended to the URL), a request body, and a timeout in seconds.";
}

std::string fetch_tool::parameters_json() const {
    return R"({
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
})";
}

std::string fetch_tool::execute(const std::string&) {
    return "[error] web fetch not supported on this platform";
}

} // namespace agent
} // namespace spiration
