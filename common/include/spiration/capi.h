/**
 * @file capi.h
 * @brief 宿主侧 C API 适配层。
 * @author clk
 *
 * 本头文件是 **宿主内部** 接口，用于把 C API（`spiration/api.h`）接到既有的
 * C++ 拓展体系上。拓展不应包含本文件。
 */

#pragma once

#include <spiration/api.h>
#include <spiration/export.h>
#include <spiration/extension/extension.hpp>
#include <spiration/extension/extension_manifest.h>

#include <memory>
#include <string>

namespace spiration {

class extension;

namespace capi {

/**
 * @return 宿主实现的 C API 函数表（进程内唯一，首次调用时初始化）。
 */
SPIRATION_EXPORT const spiration_host_api* host_api();

/**
 * @return 界面可选接口；`required_version` 不被支持时返回 `nullptr`。
 */
SPIRATION_EXPORT const spiration_ui_api* ui_interface(uint32_t required_version);

/**
 * @return 智能体可选接口；`required_version` 不被支持时返回 `nullptr`。
 */
SPIRATION_EXPORT const spiration_agent_api* agent_interface(
    uint32_t required_version);

/**
 * @brief 强制注销某拓展注册的全部 provider（用于拓展卸载清理）。
 * @param desc 拓展描述符。
 * @note 只把 provider 从智能体注册表移除并置为失效，**不**访问其 `user_data`；
 *       因此脚本类拓展仍应在关闭回调中主动注销并释放自己的状态。
 */
SPIRATION_EXPORT void release_agent_owned(const spiration_extension_desc* desc);

/**
 * @brief 释放某拓展创建的界面资源（关闭其标签页、移除其菜单项）。
 * @note 由 `forget` 调用，实现位于 `capi/tab.cpp`。
 *       若不做此清理，用户在拓展卸载后点击其按钮或菜单项，
 *       就会跳转进已卸载的动态库。
 */
SPIRATION_EXPORT void release_ui_owned(const spiration_extension_desc* desc);

/**
 * @brief 清空当前线程的结果区。
 */
SPIRATION_EXPORT void clear_results();
/**
 * @brief 结果区作用域守卫：析构时清空结果区。
 *
 * 宿主每一次进入拓展回调（`on_initialize` / `on_shutdown` / 事件回调 …）
 * 都应构造一个守卫，使该次回调期间取得的字符串在整个回调内保持有效，
 * 回调返回后统一失效。
 */
class SPIRATION_EXPORT result_scope {
public:
    result_scope() = default;
    ~result_scope();
    result_scope(const result_scope&) = delete;
    result_scope& operator=(const result_scope&) = delete;
};

/**
 * @brief 登记描述符，使其可被 `find_extension` / `enumerate_extensions` 找到。
 */
SPIRATION_EXPORT void track(spiration_extension_desc* desc);

/**
 * @brief 注销描述符，并释放宿主为其保存的全部状态
 *        （事件订阅、待执行的 `dispatch` 任务、菜单项与标签页等）。
 */
SPIRATION_EXPORT void forget(const spiration_extension_desc* desc);

/**
 * @brief 为原生 C++ 拓展构造“垫片描述符”。
 * @param ext 拓展实例。
 * @return 描述符指针，生命周期与进程一致；`ext` 为 `nullptr` 时返回 `nullptr`。
 * @note 垫片描述符只有身份字段有效，回调恒为 `nullptr`。
 */
SPIRATION_EXPORT const spiration_extension_desc* shim(extension* ext);

/**
 * @brief 为原生 C++ 拓展构造**装载期**可用的描述符。
 *
 * 与 `shim` 的区别是它不依赖已存在的 `extension` 实例：宿主在调用
 * `spiration_initialize` 之前先取得该描述符（其中 `host` 已写入），
 * 于是入口处即可用 `host->log(self, ...)`，`self->id` 也已是本拓展的标识。
 *
 * @param id      拓展 ID（取自清单）。
 * @param version 版本号字符串，可为空。
 * @return 描述符指针，生命周期与进程一致；`id` 为空时返回 `nullptr`。
 * @note 同一 `id` 重复调用返回同一描述符，仅刷新版本号。
 */
SPIRATION_EXPORT spiration_extension_desc* shim_descriptor(
    const std::string& id, const std::string& version);

/**
 * @brief 注册加载器函数表。
 * @param loader_id 加载器拓展 ID。
 * @param loader    函数表。
 */
SPIRATION_EXPORT spiration_status register_loader(
    const std::string& loader_id, const spiration_loader_api* loader);

/**
 * @brief 查询已注册的加载器。
 */
SPIRATION_EXPORT const spiration_loader_api* find_loader(
    const std::string& loader_id);

/**
 * @brief 以描述符包装出可被 `extension_manager` 管理的拓展实例。
 * @param manifest 拓展清单。
 * @param desc     描述符（所有权仍归其创建者）。
 * @param loader   若描述符由加载器创建，传入该加载器以便在关闭后归还。
 * @return 拓展实例；`desc` 非法时返回 `nullptr`。
 */
SPIRATION_EXPORT std::unique_ptr<extension> wrap(
    const manifest_data& manifest, spiration_extension_desc* desc,
    const spiration_loader_api* loader);

/**
 * @brief 尝试按 C API 装载一个拓展。
 *
 * 解析顺序：
 * 1. 清单声明了 `loader` → 交给加载器的 `instantiate`；
 * 2. 动态库导出 `spiration_extension_entry` → 调用入口函数；
 * 3. 两者皆无 → 返回 `nullptr`（调用方回退到旧 C++ ABI）。
 *
 * @param manifest      已解析的清单。
 * @param manifest_json 清单原文。
 * @param library       动态库句柄；加载器路径可为 `nullptr`。
 * @param error         失败原因（可为 `nullptr`）。
 * @return 拓展实例；失败返回 `nullptr`。
 */
SPIRATION_EXPORT std::unique_ptr<extension> try_load(
    const manifest_data& manifest, const std::string& manifest_json,
    void* library, std::string* error);

/**
 * @name 宿主函数表条目
 *
 * `spiration_host_api` 中各个函数指针的实际实现，分散在
 * `src/spiration/capi/` 下的多个翻译单元中；在此集中声明，使每一处定义
 * 都能与声明互相校验，并由 `core.cpp` 统一组装成函数表。
 *
 * @note 这些符号只在 `spiration.core` 内部使用，不是对外 ABI 的一部分，
 *       因此不加 `SPIRATION_EXPORT`。
 * @{
 */

/** @brief 定义于 `capi/memory.cpp`。 */
void* SPIRATION_CALL host_alloc(const spiration_extension_desc* self, size_t size);
void* SPIRATION_CALL host_realloc(const spiration_extension_desc* self,
                                 void* block, size_t size);
void SPIRATION_CALL host_dealloc(const spiration_extension_desc* self,
                                void* block);

/** @brief 定义于 `capi/diagnostics.cpp`。 */
void SPIRATION_CALL host_log(const spiration_extension_desc* self,
                            spiration_log_level level,
                            spiration_str_view message,
                            spiration_str_view tag);
spiration_str_view SPIRATION_CALL host_last_error(
    const spiration_extension_desc* self);
void SPIRATION_CALL host_reset_results(const spiration_extension_desc* self);

/** @brief 定义于 `capi/thread.cpp`。 */
spiration_bool SPIRATION_CALL host_is_host_thread(
    const spiration_extension_desc* self);
spiration_status SPIRATION_CALL host_dispatch(
    const spiration_extension_desc* self, spiration_callback callback,
    void* user_data);

/** @brief 定义于 `capi/path.cpp`。 */
spiration_str_view SPIRATION_CALL host_extension_path(
    const spiration_extension_desc* self, spiration_str_view relative);
spiration_str_view SPIRATION_CALL host_data_path(
    const spiration_extension_desc* self, spiration_str_view relative);

/** @brief 定义于 `capi/i18n.cpp`。 */
spiration_str_view SPIRATION_CALL host_translate(
    const spiration_extension_desc* self, spiration_str_view key,
    const spiration_str_view* args, size_t arg_count);
spiration_status SPIRATION_CALL host_load_translations(
    const spiration_extension_desc* self, spiration_str_view path);

/** @brief 定义于 `capi/events.cpp`。 */
int64_t SPIRATION_CALL host_subscribe(const spiration_extension_desc* self,
                                      spiration_str_view event,
                                      spiration_event_callback callback,
                                      void* user_data);
void SPIRATION_CALL host_unsubscribe(const spiration_extension_desc* self,
                                     int64_t subscription);
spiration_status SPIRATION_CALL host_publish(
    const spiration_extension_desc* self, spiration_str_view event,
    spiration_str_view data);

/** @brief 定义于 `capi/services.cpp`。 */
spiration_status SPIRATION_CALL host_register_service(
    const spiration_extension_desc* self, spiration_str_view name,
    void* service);
void* SPIRATION_CALL host_query_service(const spiration_extension_desc* self,
                                        spiration_str_view extension_id,
                                        spiration_str_view name);

/** @brief 定义于 `capi/registry.cpp`。 */
spiration_extension_desc* SPIRATION_CALL host_find_extension(
    const spiration_extension_desc* self, spiration_str_view id);
size_t SPIRATION_CALL host_enumerate_extensions(
    const spiration_extension_desc* self,
    const spiration_extension_desc** out, size_t capacity);

/** @brief 定义于 `capi/loader.cpp`。 */
spiration_status SPIRATION_CALL host_register_loader(
    const spiration_extension_desc* self,
    const spiration_loader_api* loader);
const spiration_loader_api* SPIRATION_CALL host_query_loader(
    const spiration_extension_desc* self, spiration_str_view loader_id);

/** @brief 定义于 `capi/ui.cpp`。 */
const void* SPIRATION_CALL host_query_interface(
    const spiration_extension_desc* self, spiration_str_view name,
    uint32_t required_version);

/** @} */

} // namespace capi
} // namespace spiration
