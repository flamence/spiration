/**
 * @file core.cpp
 * @brief C API 适配层的共享状态、结果区与宿主函数表。
 * @author clk
 *
 * 本文件是 `capi/` 目录的公共基础：
 *
 * - 线程结果区与最近错误（`host->last_error` / `reset_results` 的落点）；
 * - 适配层全局状态单例与存活标志表；
 * - 应用/管理器接入点；
 * - `spiration_host_api` 的组装。
 *
 * 各子系统的实现分散在：
 *
 * ```
 * memory.cpp       一、内存
 * diagnostics.cpp  二、诊断
 * thread.cpp       三、线程
 * path.cpp         四、路径
 * i18n.cpp         五、国际化
 * events.cpp       六、事件
 * services.cpp     七、拓展间服务
 * registry.cpp     八、拓展注册表
 * ui.cpp           九、可选接口（查询入口）
 * loader.cpp       十、加载器
 * extension.cpp    描述符包装与装载
 * ```
 */

#include "internal.h"

#include <spiration/application.h>
#include <spiration/extension/extension_manager.h>

#include <cstring>
#include <deque>
#include <string>
#include <thread>

namespace spiration {
namespace capi {

namespace {

/** 当前线程的结果区；`std::deque` 保证既有元素的引用不因追加而失效。 */
thread_local std::deque<std::string> g_results;
/** 当前线程的最近错误。 */
thread_local std::string g_last_error;

} // namespace

state& global_state() {
    static state instance;
    return instance;
}

std::mutex& state_mutex() {
    static std::mutex mtx;
    return mtx;
}

std::shared_ptr<std::atomic<bool>> alive_flag(
    const spiration_extension_desc* self) {
    std::lock_guard<std::mutex> lock(state_mutex());
    auto it = global_state().alive.find(self);
    if (it == global_state().alive.end()) return nullptr;
    return it->second;
}

spiration_str_view store(std::string text) {
    g_results.push_back(std::move(text));
    const std::string& back = g_results.back();
    spiration_str_view out;
    out.data = back.data();
    out.length = back.size();
    return out;
}

spiration_str_view last_error_text() {
    return view_of(g_last_error);
}

void set_last_error(std::string message) {
    g_last_error = std::move(message);
}

spiration_status fail(spiration_status status, std::string message) {
    g_last_error = std::move(message);
    return status;
}

std::shared_ptr<logging::logger> logger() {
    application* app = application::instance();
    return app ? app->logger() : nullptr;
}

extension_manager* manager() {
    application* app = application::instance();
    return app ? app->extension().get() : nullptr;
}

const std::thread::id& host_thread() {
    static const std::thread::id id = std::this_thread::get_id();
    return id;
}

void clear_results() {
    g_results.clear();
    g_last_error.clear();
}

result_scope::~result_scope() {
    clear_results();
}

/* -------------------------------------------------------------------------
 * 宿主函数表
 * ---------------------------------------------------------------------- */

/* 表内各函数指针的声明集中在 `spiration/capi.h`，实现分散在同目录的
   多个 `.cpp` 中；此处只负责把它们组装成 `spiration_host_api`。 */

/** @brief 组装宿主函数表。 */
spiration_host_api make_host_api() {
    spiration_host_api api;
    std::memset(&api, 0, sizeof(api));

    api.version = SPIRATION_API_VERSION;

    api.alloc = host_alloc;
    api.realloc = host_realloc;
    api.dealloc = host_dealloc;

    api.log = host_log;
    api.last_error = host_last_error;
    api.reset_results = host_reset_results;

    api.is_host_thread = host_is_host_thread;
    api.dispatch = host_dispatch;

    api.extension_path = host_extension_path;
    api.data_path = host_data_path;

    api.translate = host_translate;
    api.load_translations = host_load_translations;

    api.subscribe = host_subscribe;
    api.unsubscribe = host_unsubscribe;
    api.publish = host_publish;

    api.register_service = host_register_service;
    api.query_service = host_query_service;

    api.find_extension = host_find_extension;
    api.enumerate_extensions = host_enumerate_extensions;

    api.query_interface = host_query_interface;

    api.register_loader = host_register_loader;
    api.query_loader = host_query_loader;

    api.size = static_cast<uint32_t>(sizeof(api));
    return api;
}

const void* SPIRATION_CALL host_query_interface(
    const spiration_extension_desc* self, spiration_str_view name,
    uint32_t required_version) {
    SPIRATION_UNUSED(self);
    const std::string interface_name = text_of(name);
    if (interface_name == SPIRATION_UI_INTERFACE_NAME) {
        return ui_interface(required_version);
    }
    return NULL;
}

const spiration_host_api* host_api() {
    /* 首次调用发生在宿主主线程（拓展装载期间），顺带固定主线程 ID。 */
    host_thread();
    static const spiration_host_api api = make_host_api();
    return &api;
}

} // namespace capi
} // namespace spiration
