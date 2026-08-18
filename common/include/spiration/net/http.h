/**
 * @file http.h
 * @brief 平台无关的 HTTP 网络请求抽象。
 *
 * 桌面平台（Windows / Linux / macOS）使用 libcurl 实现，
 * OHOS 平台使用 Remote Communication Kit（@ohos.net.http，经 NAPI 桥接）。
 * 业务代码只依赖本接口，通过 spiration::net::http() 获取当前平台的实现。
 * @author clk
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace spiration {
namespace net {

/// @brief HTTP 请求。
struct http_request {
    std::string method = "GET";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    long timeout_seconds = 30;
    bool follow_redirects = true;
    std::string user_agent = "spiration/1.0";
};

/// @brief HTTP 响应。
struct http_response {
    long status = 0;
    std::string body;
    /// @brief 非空表示传输层错误（连接失败、超时等）。
    std::string error;
};

/// @brief 流式数据块回调（SSE 等分块响应）。
using http_data_callback = std::function<void(const char* data, size_t size)>;
/// @brief 可选取消条件：返回 true 表示应停止接收。
using http_stop_callback = std::function<bool()>;

/// @brief 平台 HTTP 客户端抽象。
class http_client {
public:
    virtual ~http_client() = default;

    /// @brief 发送一次非流式请求，阻塞直到响应完成或出错。
    virtual http_response request(const http_request& req) = 0;

    /**
     * @brief 发送流式请求（SSE）。收到数据块时回调 on_data，直至完成/出错/取消。
     * @param on_data     数据块回调
     * @param should_stop 可选取消条件
     */
    virtual http_response request_stream(const http_request& req,
                                         const http_data_callback& on_data,
                                         const http_stop_callback& should_stop) = 0;
};

/// @brief 获取当前平台的 HTTP 客户端。
///        未注册时返回一个始终报错的回退实现。
http_client& http();

/// @brief 注册平台 HTTP 客户端实现（平台启动时调用，覆盖默认回退）。
void set_http_client(std::unique_ptr<http_client> client);

} // namespace net
} // namespace spiration
