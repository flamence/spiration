/**
 * @file http.cpp
 * @brief HTTP 抽象注册表实现。
 *
 * 桌面平台默认懒创建 curl 后端；OHOS 平台由 entry 目标在
 * NAPI 初始化时通过 set_http_client() 注册 Remote Communication Kit 后端。
 * @author clk
 */

#include <spiration/net/http.h>

#include <memory>
#include <mutex>

#ifndef __OHOS__
#include <spiration/net/curl_http.h>
#endif

namespace spiration {
namespace net {

namespace {

/// @brief 未注册平台实现时的回退：直接返回错误，避免空指针。
class null_http : public http_client {
public:
    http_response request(const http_request& req) override {
        (void)req;
        return {0, "", "http client not registered on this platform"};
    }
    http_response request_stream(const http_request& req,
                                 const http_data_callback& on_data,
                                 const http_stop_callback& should_stop) override {
        (void)req; (void)on_data; (void)should_stop;
        return {0, "", "http client not registered on this platform"};
    }
};

std::unique_ptr<http_client>& client_slot() {
    static std::unique_ptr<http_client> client;
    return client;
}

std::mutex& client_mutex() {
    static std::mutex mtx;
    return mtx;
}

} // namespace

http_client& http() {
    std::lock_guard<std::mutex> lk(client_mutex());
    auto& slot = client_slot();
    if (!slot) {
#ifndef __OHOS__
        slot = std::make_unique<curl_http>();
#else
        slot = std::make_unique<null_http>();
#endif
    }
    return *slot;
}

void set_http_client(std::unique_ptr<http_client> client) {
    std::lock_guard<std::mutex> lk(client_mutex());
    client_slot() = std::move(client);
}

} // namespace net
} // namespace spiration
