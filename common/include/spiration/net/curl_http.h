/**
 * @file curl_http.h
 * @brief 基于 libcurl 的 HTTP 客户端（桌面平台）。
 * @author clk
 */

#pragma once

#include <spiration/net/http.h>

namespace spiration {
namespace net {

/// @brief 流式写回调的接收对象。
struct http_stream_sink {
    http_data_callback on_data;
    http_stop_callback should_stop;
};

/// @brief libcurl 实现。
class curl_http : public http_client {
public:
    http_response request(const http_request& req) override;
    http_response request_stream(const http_request& req,
                                 const http_data_callback& on_data,
                                 const http_stop_callback& should_stop) override;
};

} // namespace net
} // namespace spiration
