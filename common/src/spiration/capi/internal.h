/**
 * @file internal.h
 * @brief C API 适配层的内部共享设施（宿主私有）。
 * @author clk
 *
 * 本头文件取代了原先的单文件工具头 `capi_util.h`：除字符串转换外，还把
 * **跨翻译单元共享的状态与结果区**一并收拢在此，使 `capi/` 目录下的实现
 * 文件可以按职责拆分而不必互相包含 .cpp。
 *
 * @note 本头文件仅供 `spiration.core` 内部使用，不随 SDK 安装。
 */

#pragma once

#include <spiration/api.h>
#include <spiration/capi.h>
#include <spiration/extension/extension_manifest.h>
#include <spiration/logging/logger.hpp>

#include <atomic>
#include <ghc/filesystem.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace spiration {

class extension;
class extension_manager;

namespace capi {

/**
 * @brief 适配层全局状态。
 *
 * @note 成员的生命周期与进程一致；`alive` 受 `state_mutex()` 保护，
 *       其余成员按 C API 的线程模型只在宿主主线程访问。
 */
struct state {
    /** 已登记的描述符：拓展 ID → 描述符。 */
    std::map<std::string, spiration_extension_desc*> descriptors;
    /** 已注册的加载器：加载器 ID → 函数表。 */
    std::map<std::string, const spiration_loader_api*> loaders;
    /** 拓展存活标志，供 `dispatch` 的回调在关闭后自动失效。 */
    std::map<const spiration_extension_desc*,
             std::shared_ptr<std::atomic<bool>>>
        alive;
    /** 拓展持有的订阅 ID，关闭时统一退订。 */
    std::map<const spiration_extension_desc*, std::vector<int64_t>>
        subscriptions;
};

/** @brief 取适配层状态单例。 */
state& global_state();

/** @brief 保护 `state::alive`（仅 `dispatch` 需要跨线程取用）。 */
std::mutex& state_mutex();

/** @brief 取拓展的存活标志（可跨线程安全取用）。 */
std::shared_ptr<std::atomic<bool>> alive_flag(
    const spiration_extension_desc* self);

/* -------------------------------------------------------------------------
 * 字符串
 * ---------------------------------------------------------------------- */

/** @brief 字符串视图 → `std::string`。 */
inline std::string text_of(spiration_str_view text) {
    return text.data ? std::string(text.data, text.length) : std::string();
}

/** @brief `std::string` → 字符串视图（借用，调用方保证生命周期）。 */
inline spiration_str_view view_of(const std::string& text) {
    spiration_str_view out;
    out.data = text.empty() ? NULL : text.data();
    out.length = text.size();
    return out;
}

/** @brief 空字符串视图。 */
inline spiration_str_view empty_view() {
    spiration_str_view out;
    out.data = NULL;
    out.length = 0u;
    return out;
}

/** @brief 取拓展短名（`com.example.hello-world` → `hello-world`）。 */
inline std::string simple_id(const std::string& id) {
    const size_t pos = id.find_last_of('.');
    return pos == std::string::npos ? id : id.substr(pos + 1);
}

/* -------------------------------------------------------------------------
 * 结果区与错误
 * ---------------------------------------------------------------------- */

/**
 * @brief 把文本存入本线程结果区并返回视图。
 * @note 结果区随其后取得的视图一同失效，详见 `spiration/api/host.h`。
 */
spiration_str_view store(std::string text);

/** @brief 本线程最近一次错误的文本视图（指向线程局部存储）。 */
spiration_str_view last_error_text();

/**
 * @brief 只记录错误文本，不产生状态码。
 * @note 供「以空串 / `NULL` 表示失败」的函数使用。
 */
void set_last_error(std::string message);

/** @brief 记录最近一次错误并返回状态码。 */
spiration_status fail(spiration_status status, std::string message);

/* -------------------------------------------------------------------------
 * 宿主接入点
 * ---------------------------------------------------------------------- */

/** @brief 取日志器；应用未就绪时返回空。 */
std::shared_ptr<logging::logger> logger();

/** @brief 取拓展管理器；应用未就绪时返回空。 */
extension_manager* manager();

/** @brief 宿主主线程 ID（首次取用时记录）。 */
const std::thread::id& host_thread();

/* -------------------------------------------------------------------------
 * 路径
 * ---------------------------------------------------------------------- */

/**
 * @brief 拼接相对路径并阻止逃逸出根目录。
 * @return 越界、非法或根目录为空时返回空路径。
 * @note 定义于 `path.cpp`；`i18n.cpp` 亦用它校验语言文件路径。
 */
ghc::filesystem::path join_safe(const ghc::filesystem::path& base,
                                const std::string& relative);

/**
 * @brief 拓展安装目录。
 * @return 应用或描述符不可用时返回空路径。
 * @note 定义于 `path.cpp`。
 */
ghc::filesystem::path extension_root(const spiration_extension_desc* self);

/* -------------------------------------------------------------------------
 * 描述符登记与垫片
 * ---------------------------------------------------------------------- */

/** @brief 登记描述符，使其可被 `find_extension` / `enumerate_extensions` 找到。 */
void track(spiration_extension_desc* desc);

/**
 * @brief 为原生 C++ 拓展构造“垫片描述符”。
 * @note 垫片只有身份字段有效，回调恒为空。
 */
const spiration_extension_desc* shim(extension* ext);

/* -------------------------------------------------------------------------
 * 加载器
 * ---------------------------------------------------------------------- */

/** @brief 注册加载器函数表。 */
spiration_status register_loader(const std::string& loader_id,
                                 const spiration_loader_api* loader);

/** @brief 查询已注册的加载器；不存在返回空。 */
const spiration_loader_api* find_loader(const std::string& loader_id);

/* -------------------------------------------------------------------------
 * 描述符包装
 * ---------------------------------------------------------------------- */

/**
 * @brief 以描述符包装出可被 `extension_manager` 管理的拓展实例。
 * @param manifest 拓展清单。
 * @param desc     描述符（所有权仍归其创建者）。
 * @param loader   若描述符由加载器创建，传入该加载器以便在关闭后归还。
 */
std::unique_ptr<extension> wrap(const manifest_data& manifest,
                                spiration_extension_desc* desc,
                                const spiration_loader_api* loader);

} // namespace capi
} // namespace spiration
