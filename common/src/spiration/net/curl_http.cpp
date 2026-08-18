/**
 * @file curl_http.cpp
 * @brief 基于 libcurl 的 HTTP 客户端实现（桌面平台）。
 * @author clk
 */

#include <spiration/net/curl_http.h>

#include <curl/curl.h>

#include <cctype>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace spiration {
namespace net {

namespace {

size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

size_t stream_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* sink = static_cast<http_stream_sink*>(userp);
    if (!sink) return total;
    if (sink->should_stop && sink->should_stop()) return 0;
    if (sink->on_data) sink->on_data(static_cast<char*>(contents), total);
    return total;
}

std::string upper_copy(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

http_response curl_http::request(const http_request& req) {
    CURL* curl = curl_easy_init();
    if (!curl) return {0, "", "curl_easy_init failed"};

    std::string response;
    curl_slist* hdr_list = nullptr;
    for (const auto& h : req.headers) {
        if (h.first.empty()) continue;
        hdr_list = curl_slist_append(hdr_list, (h.first + ": " + h.second).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    if (req.follow_redirects) curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, req.timeout_seconds);
    if (!req.user_agent.empty())
        curl_easy_setopt(curl, CURLOPT_USERAGENT, req.user_agent.c_str());
    if (hdr_list) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    std::string m = upper_copy(req.method);
    if (m == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (m == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body.size()));
    } else if (m == "PUT" || m == "PATCH" || m == "DELETE" || m == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, m.c_str());
        if (!req.body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body.size()));
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, m.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (hdr_list) curl_slist_free_all(hdr_list);
    curl_easy_cleanup(curl);

    http_response out;
    out.status = status;
    out.body = std::move(response);
    if (res != CURLE_OK) out.error = curl_easy_strerror(res);
    return out;
}

http_response curl_http::request_stream(const http_request& req,
                                        const http_data_callback& on_data,
                                        const http_stop_callback& should_stop) {
    CURL* curl = curl_easy_init();
    if (!curl) return {0, "", "curl_easy_init failed"};

    http_stream_sink sink;
    sink.on_data = on_data;
    sink.should_stop = should_stop;

    curl_slist* hdr_list = nullptr;
    for (const auto& h : req.headers) {
        if (h.first.empty()) continue;
        hdr_list = curl_slist_append(hdr_list, (h.first + ": " + h.second).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    if (req.follow_redirects) curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, req.timeout_seconds);
    if (!req.user_agent.empty())
        curl_easy_setopt(curl, CURLOPT_USERAGENT, req.user_agent.c_str());
    if (hdr_list) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);

    std::string m = upper_copy(req.method);
    if (m == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (m == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body.size()));
    } else if (m == "PUT" || m == "PATCH" || m == "DELETE" || m == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, m.c_str());
        if (!req.body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body.size()));
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, m.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (hdr_list) curl_slist_free_all(hdr_list);
    curl_easy_cleanup(curl);

    http_response out;
    out.status = status;
    if (res != CURLE_OK) out.error = curl_easy_strerror(res);
    return out;
}

} // namespace net
} // namespace spiration
