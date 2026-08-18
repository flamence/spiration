/**
 * @file ohos_http.cpp
 * @brief OHOS HTTP 客户端实现（NAPI 桥接 Remote Communication Kit）。
 *
 * 工作方式：
 *  - C++ 后台线程（chat/web 工具）发起请求，经 napi_threadsafe_function
 *    投递到 ArkTS 主线程；
 *  - ArkTS 侧（Index.ets 注册的 handler）使用 @ohos.net.http 执行请求；
 *  - 数据/完成经 httpStreamData / httpStreamEnd / httpRequestDone 回传 C++，
 *    等待线程通过条件变量唤醒。
 * @author clk
 */

#include <spiration/net/ohos_http.h>

#include <napi/native_api.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace spiration {
namespace net {

namespace {

/// @brief 一次请求的共享状态：JS 线程写入，等待线程读取。
struct pending_request {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    long status = 0;
    std::string body;
    std::string error;
    http_data_callback on_data;
    http_stop_callback should_stop;
};

/// @brief 经 TSFN 投递给 ArkTS 线程的任务。
struct http_task {
    enum class kind { request, cancel };
    kind type = kind::request;
    uint64_t id = 0;
    std::string method = "GET";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    long timeout_seconds = 30;
    std::string user_agent = "spiration/1.0";
    bool stream = false;
};

// ---- 全局桥接状态（app 生命周期内有效） ----
napi_threadsafe_function g_tsfn = nullptr;
std::atomic<uint64_t> g_next_id{1};

std::mutex g_pending_mtx;
std::unordered_map<uint64_t, std::shared_ptr<pending_request>> g_pending;

// ---- 小工具 ----

napi_value make_string(napi_env env, const std::string& s) {
    napi_value v;
    napi_create_string_utf8(env, s.c_str(), s.size(), &v);
    return v;
}

napi_value make_number(napi_env env, double d) {
    napi_value v;
    napi_create_double(env, d, &v);
    return v;
}

napi_value make_bool(napi_env env, bool b) {
    napi_value v;
    napi_get_boolean(env, b, &v);
    return v;
}

napi_value make_undefined(napi_env env) {
    napi_value v;
    napi_get_undefined(env, &v);
    return v;
}

double number_arg(napi_env env, napi_value v) {
    double d = 0;
    napi_get_value_double(env, v, &d);
    return d;
}

std::string string_arg(napi_env env, napi_value v) {
    size_t len = 0;
    if (napi_get_value_string_utf8(env, v, nullptr, 0, &len) != napi_ok) return "";
    if (len == 0) return "";
    std::string s(len, '\0');
    napi_get_value_string_utf8(env, v, &s[0], len + 1, &len);
    s.resize(len);
    return s;
}

// ---- pending 注册表 ----

void register_pending(uint64_t id, const std::shared_ptr<pending_request>& p) {
    std::lock_guard<std::mutex> lk(g_pending_mtx);
    g_pending[id] = p;
}

std::shared_ptr<pending_request> find_pending(uint64_t id) {
    std::lock_guard<std::mutex> lk(g_pending_mtx);
    auto it = g_pending.find(id);
    return it == g_pending.end() ? nullptr : it->second;
}

std::shared_ptr<pending_request> take_pending(uint64_t id) {
    std::lock_guard<std::mutex> lk(g_pending_mtx);
    auto it = g_pending.find(id);
    if (it == g_pending.end()) return nullptr;
    auto p = it->second;
    g_pending.erase(it);
    return p;
}

void remove_pending(uint64_t id) {
    std::lock_guard<std::mutex> lk(g_pending_mtx);
    g_pending.erase(id);
}

// ---- 桥接实现 ----

/// @brief 在 ArkTS 主线程执行：将任务参数组为对象并调用注册的 handler。
static void HttpCallJs(napi_env env, napi_value js_cb, void*, void* data) {
    auto* task = static_cast<http_task*>(data);
    if (!task) return;

    napi_value arg;
    napi_create_object(env, &arg);
    napi_set_named_property(env, arg, "id",
                            make_number(env, static_cast<double>(task->id)));
    napi_set_named_property(env, arg, "kind",
                            make_string(env, task->type == http_task::kind::cancel ? "cancel" : "request"));
    napi_set_named_property(env, arg, "method", make_string(env, task->method));
    napi_set_named_property(env, arg, "url", make_string(env, task->url));

    napi_value arr;
    napi_create_array_with_length(env, task->headers.size(), &arr);
    for (size_t i = 0; i < task->headers.size(); ++i) {
        napi_value obj;
        napi_create_object(env, &obj);
        napi_set_named_property(env, obj, "name", make_string(env, task->headers[i].first));
        napi_set_named_property(env, obj, "value", make_string(env, task->headers[i].second));
        napi_set_element(env, arr, static_cast<uint32_t>(i), obj);
    }
    napi_set_named_property(env, arg, "headers", arr);

    napi_set_named_property(env, arg, "body", make_string(env, task->body));
    napi_set_named_property(env, arg, "timeout",
                            make_number(env, static_cast<double>(task->timeout_seconds)));
    napi_set_named_property(env, arg, "userAgent", make_string(env, task->user_agent));
    napi_set_named_property(env, arg, "stream", make_bool(env, task->stream));

    napi_value undefined;
    napi_get_undefined(env, &undefined);
    napi_call_function(env, undefined, js_cb, 1, &arg, nullptr);
    delete task;
}

void post_cancel(uint64_t id) {
    if (!g_tsfn) return;
    auto* task = new http_task;
    task->type = http_task::kind::cancel;
    task->id = id;
    napi_call_threadsafe_function(g_tsfn, task, napi_tsfn_nonblocking);
}

http_response do_request(const http_request& req,
                         const http_data_callback& on_data,
                         const http_stop_callback& should_stop) {
    if (!g_tsfn) return {0, "", "http handler not registered on ArkTS side"};

    auto pending = std::make_shared<pending_request>();
    pending->on_data = on_data;
    pending->should_stop = should_stop;

    uint64_t id = g_next_id.fetch_add(1, std::memory_order_relaxed);
    register_pending(id, pending);

    auto* task = new http_task;
    task->id = id;
    task->method = req.method;
    task->url = req.url;
    task->headers = req.headers;
    task->body = req.body;
    task->timeout_seconds = req.timeout_seconds;
    task->user_agent = req.user_agent;
    task->stream = static_cast<bool>(on_data);

    napi_status st = napi_call_threadsafe_function(g_tsfn, task, napi_tsfn_nonblocking);
    if (st != napi_ok) {
        delete task;
        remove_pending(id);
        return {0, "", "failed to post http request to ArkTS"};
    }

    long timeout = req.timeout_seconds > 0 ? req.timeout_seconds : 30;
    // 等待：总超时 + 缓冲，避免 JS 侧迟迟未回调时永久阻塞。
    std::unique_lock<std::mutex> lk(pending->mtx);
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(timeout) + std::chrono::seconds(15);
    pending->cv.wait_until(lk, deadline, [&]() {
        return pending->done || (pending->should_stop && pending->should_stop());
    });

    if (!pending->done) {
        bool stopped = pending->should_stop && pending->should_stop();
        if (stopped) post_cancel(id);
        remove_pending(id);
        return {0, "", stopped ? "cancelled" : "timeout"};
    }

    http_response resp;
    resp.status = pending->status;
    resp.body = std::move(pending->body);
    resp.error = std::move(pending->error);
    return resp;
}

} // namespace

// ---- http_client 实现 ----

http_response ohos_http::request(const http_request& req) {
    return do_request(req, {}, {});
}

http_response ohos_http::request_stream(const http_request& req,
                                        const http_data_callback& on_data,
                                        const http_stop_callback& should_stop) {
    return do_request(req, on_data, should_stop);
}

void register_ohos_http() {
    set_http_client(std::make_unique<ohos_http>());
}

} // namespace net
} // namespace spiration

namespace spiration {
namespace bridge {

// ---- NAPI 导出（供 ArkTS 调用） ----

napi_value NapiRegisterHttpHandler(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 1 || net::g_tsfn) return net::make_undefined(env);

    napi_value resource_name;
    napi_create_string_utf8(env, "spiration_http", NAPI_AUTO_LENGTH, &resource_name);
    napi_create_threadsafe_function(env, args[0], nullptr, resource_name,
                                    0 /* 不限队列 */, 1, nullptr, nullptr,
                                    nullptr, net::HttpCallJs, &net::g_tsfn);
    return args[0];
}

napi_value NapiHttpStreamData(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 3) return net::make_undefined(env);

    uint64_t id = static_cast<uint64_t>(net::number_arg(env, args[0]));
    std::string chunk = net::string_arg(env, args[2]);
    auto pending = net::find_pending(id);
    if (!pending) return net::make_undefined(env);

    {
        std::lock_guard<std::mutex> lk(pending->mtx);
        pending->body += chunk;
        if (pending->should_stop && pending->should_stop()) {
            pending->error = "cancelled";
            pending->done = true;
            pending->cv.notify_all();
        }
    }
    if (!chunk.empty() && pending->on_data) {
        pending->on_data(chunk.data(), chunk.size());
    }
    return net::make_undefined(env);
}

napi_value NapiHttpStreamEnd(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 3) return net::make_undefined(env);

    uint64_t id = static_cast<uint64_t>(net::number_arg(env, args[0]));
    long status = static_cast<long>(net::number_arg(env, args[1]));
    std::string error = net::string_arg(env, args[2]);

    auto pending = net::take_pending(id);
    if (!pending) return net::make_undefined(env);
    {
        std::lock_guard<std::mutex> lk(pending->mtx);
        pending->status = status;
        pending->error = std::move(error);
        pending->done = true;
        pending->cv.notify_all();
    }
    return net::make_undefined(env);
}

napi_value NapiHttpRequestDone(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 4) return net::make_undefined(env);

    uint64_t id = static_cast<uint64_t>(net::number_arg(env, args[0]));
    long status = static_cast<long>(net::number_arg(env, args[1]));
    std::string body = net::string_arg(env, args[2]);
    std::string error = net::string_arg(env, args[3]);

    auto pending = net::take_pending(id);
    if (!pending) return net::make_undefined(env);
    {
        std::lock_guard<std::mutex> lk(pending->mtx);
        pending->status = status;
        pending->body = std::move(body);
        pending->error = std::move(error);
        pending->done = true;
        pending->cv.notify_all();
    }
    return net::make_undefined(env);
}

} // namespace bridge
} // namespace spiration
