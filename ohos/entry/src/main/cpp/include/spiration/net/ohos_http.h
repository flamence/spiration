/**
 * @file ohos_http.h
 * @brief OHOS HTTP 客户端（经 NAPI 桥接 Remote Communication Kit）。
 * @author clk
 */

#pragma once

#include <spiration/net/http.h>

namespace spiration {
namespace net {

/// @brief OHOS 平台 HTTP 实现：请求经 napi_threadsafe_function 投递到
///        ArkTS 主线程，由 Remote Communication Kit（@ohos.net.http）执行。
class ohos_http : public http_client {
public:
    http_response request(const http_request& req) override;
    http_response request_stream(const http_request& req,
                                 const http_data_callback& on_data,
                                 const http_stop_callback& should_stop) override;
};

/// @brief 在 NAPI 初始化时注册 ohos_http 到 net::http()。
void register_ohos_http();

} // namespace net
} // namespace spiration
