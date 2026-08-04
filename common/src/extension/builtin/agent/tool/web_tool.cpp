/**
 * @file web_tool.cpp
 * @brief 互联网工具集实现。
 * @author clk
 */

#include <extension/builtin/agent/tool/web_tool.h>
#include <utils/console.h>

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

namespace spiration {
namespace agent {

namespace {

size_t fetch_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    if (out->size() + total > 65536) {
        out->append(static_cast<char*>(contents), 65536 - out->size());
    } else {
        out->append(static_cast<char*>(contents), total);
    }
    return total;
}

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

std::string upper_copy(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

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

    CURL* curl = curl_easy_init();
    if (!curl) return "[error] curl_easy_init failed";

    std::string response;
    curl_slist* hdr_list = nullptr;
    for (const auto& h : headers) {
        if (h.first.empty()) continue;
        hdr_list = curl_slist_append(hdr_list, (h.first + ": " + h.second).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "spiration/1.0");
    if (hdr_list) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fetch_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    std::string m = upper_copy(method);
    if (m == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (m == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else if (m == "PUT" || m == "PATCH" || m == "DELETE" || m == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (hdr_list) curl_slist_free_all(hdr_list);
    curl_easy_cleanup(curl);

    std::string out = "[status " + std::to_string(status) + "]";
    if (res != CURLE_OK) {
        out += " [error] " + std::string(curl_easy_strerror(res));
    }
    if (!response.empty()) {
        out += "\n" + response;
    }
    if (out.size() > 65536) out = out.substr(0, 65536) + "\n[output truncated]";
    console::info("extension/agent/web", "fetch %s -> status %ld, %zu bytes", full_url.c_str(), status, response.size());
    return out;
}

} // namespace agent
} // namespace spiration
