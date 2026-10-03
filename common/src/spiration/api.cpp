/**
 * @file api.cpp
 * @brief Spiration C API 的宿主侧实现（`spiration_host_api`）。
 * @author clk
 *
 * 本文件把 C ABI 的函数表映射到既有的 C++ 实现：
 *
 * ```
 * spiration_host_api（C） → extension_manager / i18n / logging（C++）
 * ```
 *
 * 设计要点见仓库根目录 `DESIGN.md`：
 * - 跨边界只传 C 数据与 UTF-8 文本；
 * - 宿主返回的字符串来自“线程结果区”，回调作用域结束时统一失效；
 * - 除 `dispatch` 外，全部函数只允许在宿主主线程调用。
 */

#include <spiration/capi.h>

#include "capi_util.h"

#include <spiration/api.h>
#include <spiration/application.h>
#include <spiration/extension/extension_manager.h>
#include <spiration/extension/extension_manifest.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/logging/logger.hpp>
#include <spiration/utils/dispatcher.h>

#include <ghc/filesystem.hpp>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace spiration {
namespace capi {

namespace {

using detail::empty_view;
using detail::simple_id;
using detail::text_of;
using detail::view_of;

// ---------------------------------------------------------------------------
// 结果区与错误
// ---------------------------------------------------------------------------

/** 当前线程的结果区；`std::deque` 保证既有元素的引用不因追加而失效。 */
thread_local std::deque<std::string> g_results;
/** 当前线程的最近错误。 */
thread_local std::string g_last_error;

/** @brief 把文本存入结果区并返回视图。 */
spiration_str_view store(std::string text) {
    g_results.push_back(std::move(text));
    const std::string& back = g_results.back();
    spiration_str_view out;
    out.data = back.data();
    out.length = back.size();
    return out;
}

/** @brief 记录错误并返回状态码。 */
spiration_status fail(spiration_status status, std::string message) {
    g_last_error = std::move(message);
    return status;
}

/** @brief 取日志器。 */
std::shared_ptr<logging::logger> logger() {
    application* app = application::instance();
    return app ? app->logger() : nullptr;
}

/** @brief 取拓展管理器。 */
extension_manager* manager() {
    application* app = application::instance();
    return app ? app->extension().get() : nullptr;
}

// ---------------------------------------------------------------------------
// 路径
// ---------------------------------------------------------------------------

/**
 * @brief 拼接相对路径并阻止逃逸出根目录。
 * @return 越界或非法时返回空路径。
 */
ghc::filesystem::path join_safe(const ghc::filesystem::path& base,
                                const std::string& relative) {
    if (base.empty()) return ghc::filesystem::path();

    ghc::filesystem::path joined = base;
    if (!relative.empty()) joined /= relative;
    joined = joined.lexically_normal();

    const ghc::filesystem::path root = base.lexically_normal();
    const std::string root_text = root.string();
    const std::string joined_text = joined.string();
    if (joined_text.size() < root_text.size() ||
        joined_text.compare(0, root_text.size(), root_text) != 0) {
        return ghc::filesystem::path();
    }
    return joined;
}

/** @brief 拓展安装目录。 */
ghc::filesystem::path extension_root(const spiration_extension_desc* self) {
    extension_manager* mgr = manager();
    if (mgr == nullptr || self == nullptr) return ghc::filesystem::path();
    return mgr->extension_directory(text_of(self->id));
}

/** @brief 拓展私有数据目录。 */
ghc::filesystem::path data_root(const spiration_extension_desc* self) {
    application* app = application::instance();
    if (app == nullptr || self == nullptr) return ghc::filesystem::path();
    return app->data_directory(text_of(self->id));
}

// ---------------------------------------------------------------------------
// 宿主主线程
// ---------------------------------------------------------------------------

/** @brief 宿主主线程 ID（首次取用时记录）。 */
const std::thread::id& host_thread() {
    static const std::thread::id id = std::this_thread::get_id();
    return id;
}

// ---------------------------------------------------------------------------
// 适配状态
// ---------------------------------------------------------------------------

/** @brief 原生 C++ 拓展的垫片描述符持有者。 */
struct shim_holder {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    spiration_extension_desc desc;
};

/** @brief C++ `spiration::init_phase` → C 阶段。 */
spiration_init_phase to_capi_phase(init_phase phase) {
    switch (phase) {
        case init_phase::early: return SPIRATION_PHASE_EARLY;
        case init_phase::late: return SPIRATION_PHASE_LATE;
        case init_phase::normal:
        default: return SPIRATION_PHASE_NORMAL;
    }
}

/** @brief C 阶段 → C++ `spiration::init_phase`。 */
init_phase from_capi_phase(spiration_init_phase phase) {
    switch (phase) {
        case SPIRATION_PHASE_EARLY: return init_phase::early;
        case SPIRATION_PHASE_LATE: return init_phase::late;
        case SPIRATION_PHASE_NORMAL:
        default: return init_phase::normal;
    }
}

/** @brief 适配层全局状态。 */
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
    /** 原生 C++ 拓展的垫片描述符。 */
    std::map<std::string, std::unique_ptr<shim_holder>> shims;
};

/** @brief 取适配层状态单例。 */
state& g_state() {
    static state instance;
    return instance;
}

/** @brief 保护 `alive`（仅 `dispatch` 需要跨线程取用）。 */
std::mutex& g_mutex() {
    static std::mutex mtx;
    return mtx;
}

/** @brief 取拓展的存活标志（可跨线程安全取用）。 */
std::shared_ptr<std::atomic<bool>> alive_flag(
    const spiration_extension_desc* self) {
    std::lock_guard<std::mutex> lock(g_mutex());
    auto it = g_state().alive.find(self);
    if (it == g_state().alive.end()) return nullptr;
    return it->second;
}

// ---------------------------------------------------------------------------
// 符号解析
// ---------------------------------------------------------------------------

/** @brief 在动态库中查找符号。 */
void* find_symbol(void* library, const char* name) {
    if (library == nullptr || name == nullptr) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(
        ::GetProcAddress(static_cast<HMODULE>(library), name));
#else
    return ::dlsym(library, name);
#endif
}

} // namespace

// ---------------------------------------------------------------------------
// 结果区
// ---------------------------------------------------------------------------

void clear_results() {
    g_results.clear();
    g_last_error.clear();
}

result_scope::~result_scope() {
    clear_results();
}

// ---------------------------------------------------------------------------
// 宿主函数实现
// ---------------------------------------------------------------------------

namespace {

/* ---- 一、内存 ---- */

void* SPIRATION_CALL host_alloc(const spiration_extension_desc* self,
                                size_t size) {
    SPIRATION_UNUSED(self);
    return std::malloc(size == 0u ? 1u : size);
}

void* SPIRATION_CALL host_realloc(const spiration_extension_desc* self,
                                  void* block, size_t size) {
    SPIRATION_UNUSED(self);
    return std::realloc(block, size == 0u ? 1u : size);
}

void SPIRATION_CALL host_dealloc(const spiration_extension_desc* self,
                                 void* block) {
    SPIRATION_UNUSED(self);
    std::free(block);
}

/* ---- 二、诊断 ---- */

/** @brief C 日志等级 → C++ 日志等级。 */
logging::level to_log_level(spiration_log_level level) {
    switch (level) {
        case SPIRATION_LOG_TRACE: return logging::level::trace;
        case SPIRATION_LOG_DEBUG: return logging::level::debug;
        case SPIRATION_LOG_WARNING: return logging::level::warning;
        case SPIRATION_LOG_ERROR: return logging::level::error;
        case SPIRATION_LOG_INFO:
        default: return logging::level::info;
    }
}

void SPIRATION_CALL host_log(const spiration_extension_desc* self,
                             spiration_log_level level,
                             spiration_str_view message,
                             spiration_str_view tag) {
    std::shared_ptr<logging::logger> out = logger();
    if (!out) return;

    std::string channel = "extension/";
    channel += self ? simple_id(text_of(self->id)) : "unknown";
    const std::string tag_text = text_of(tag);
    if (!tag_text.empty()) {
        channel += "/";
        channel += tag_text;
    }

    out->log({to_log_level(level), text_of(message), channel});
}

spiration_str_view SPIRATION_CALL host_last_error(
    const spiration_extension_desc* self) {
    SPIRATION_UNUSED(self);
    return view_of(g_last_error);
}

void SPIRATION_CALL host_reset_results(const spiration_extension_desc* self) {
    SPIRATION_UNUSED(self);
    clear_results();
}

/* ---- 三、线程 ---- */

spiration_bool SPIRATION_CALL host_is_host_thread(
    const spiration_extension_desc* self) {
    SPIRATION_UNUSED(self);
    return std::this_thread::get_id() == host_thread() ? SPIRATION_TRUE
                                                       : SPIRATION_FALSE;
}

spiration_status SPIRATION_CALL host_dispatch(
    const spiration_extension_desc* self, spiration_callback callback,
    void* user_data) {
    if (self == nullptr || callback == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT, "dispatch: null argument");
    }

    application* app = application::instance();
    if (app == nullptr || app->dispatcher() == nullptr) {
        return fail(SPIRATION_ERR_NOT_READY, "dispatch: dispatcher unavailable");
    }

    const std::shared_ptr<std::atomic<bool>> flag = alive_flag(self);
    app->dispatcher()->post_to_ui([flag, callback, user_data]() {
        /* 拓展已关闭则丢弃任务，保证 on_shutdown 之后不再回调。 */
        if (flag && !flag->load()) return;
        callback(user_data);
    });
    return SPIRATION_OK;
}

/* ---- 四、路径 ---- */

spiration_str_view SPIRATION_CALL host_extension_path(
    const spiration_extension_desc* self, spiration_str_view relative) {
    const ghc::filesystem::path path =
        join_safe(extension_root(self), text_of(relative));
    if (path.empty()) {
        g_last_error = "extension_path: path escapes the extension directory";
        return empty_view();
    }
    return store(path.string());
}

spiration_str_view SPIRATION_CALL host_data_path(
    const spiration_extension_desc* self, spiration_str_view relative) {
    const ghc::filesystem::path path =
        join_safe(data_root(self), text_of(relative));
    if (path.empty()) {
        g_last_error = "data_path: path escapes the data directory";
        return empty_view();
    }
    return store(path.string());
}

/* ---- 五、国际化 ---- */

spiration_str_view SPIRATION_CALL host_translate(
    const spiration_extension_desc* self, spiration_str_view key,
    const spiration_str_view* args, size_t arg_count) {
    SPIRATION_UNUSED(self);
    const std::string text_key = text_of(key);
    i18n_manager& i18n = i18n_manager::get();

    if (args != NULL && arg_count > 0u) {
        std::vector<std::string> values;
        values.reserve(arg_count);
        for (size_t i = 0; i < arg_count; ++i) {
            values.push_back(text_of(args[i]));
        }
        /* 缺失键时回退为键本身，便于定位漏翻译的条目。 */
        return store(i18n.tr(text_key, values, text_key));
    }
    return store(i18n.tr(text_key, text_key));
}

spiration_status SPIRATION_CALL host_load_translations(
    const spiration_extension_desc* self, spiration_str_view path) {
    const std::string relative = text_of(path);
    if (relative.empty()) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "load_translations: empty path");
    }

    ghc::filesystem::path target(relative);
    if (target.is_relative()) {
        target = join_safe(extension_root(self), relative);
        if (target.empty()) {
            return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                        "load_translations: path escapes the extension directory");
        }
    }
    if (!ghc::filesystem::exists(target)) {
        return fail(SPIRATION_ERR_IO, "load_translations: file not found");
    }

    i18n_manager& i18n = i18n_manager::get();
    std::string locale = i18n.get_locale();
    if (locale.empty()) locale = "zh-CN";
    if (!i18n.load(locale, target)) {
        return fail(SPIRATION_ERR_IO, "load_translations: load failed");
    }

    if (std::shared_ptr<logging::logger> out = logger()) {
        out->log({logging::level::info,
                  "loaded translations from " + target.string(),
                  "extension/" + simple_id(text_of(self->id)) + "/i18n"});
    }
    return SPIRATION_OK;
}

/* ---- 六、事件 ---- */

int64_t SPIRATION_CALL host_subscribe(const spiration_extension_desc* self,
                                      spiration_str_view event,
                                      spiration_event_callback callback,
                                      void* user_data) {
    extension_manager* mgr = manager();
    if (mgr == nullptr || self == nullptr || callback == nullptr) return 0;

    const std::string event_name = text_of(event);
    auto forward = [callback, user_data, event_name](const std::string& data) {
        callback(user_data, view_of(event_name),
                 spiration_str_view_of(data.data(), data.size()));
    };

    const int subscription = mgr->on_event(event_name, forward);
    if (subscription <= 0) return 0;

    g_state().subscriptions[self].push_back(subscription);
    return static_cast<int64_t>(subscription);
}

void SPIRATION_CALL host_unsubscribe(const spiration_extension_desc* self,
                                     int64_t subscription) {
    extension_manager* mgr = manager();
    if (mgr == nullptr || subscription <= 0) return;

    mgr->off_event(static_cast<int>(subscription));

    auto it = g_state().subscriptions.find(self);
    if (it == g_state().subscriptions.end()) return;
    std::vector<int64_t>& owned = it->second;
    for (size_t i = 0; i < owned.size(); ++i) {
        if (owned[i] == subscription) {
            owned.erase(owned.begin() + static_cast<long>(i));
            break;
        }
    }
}

spiration_status SPIRATION_CALL host_publish(
    const spiration_extension_desc* self, spiration_str_view event,
    spiration_str_view data) {
    SPIRATION_UNUSED(self);
    extension_manager* mgr = manager();
    if (mgr == nullptr) {
        return fail(SPIRATION_ERR_NOT_READY, "publish: manager unavailable");
    }
    mgr->emit_event(text_of(event), text_of(data));
    return SPIRATION_OK;
}

/* ---- 七、服务 ---- */

spiration_status SPIRATION_CALL host_register_service(
    const spiration_extension_desc* self, spiration_str_view name,
    void* service) {
    if (self == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_service: null descriptor");
    }
    extension_manager* mgr = manager();
    if (mgr == nullptr) {
        return fail(SPIRATION_ERR_NOT_READY, "register_service: unavailable");
    }
    mgr->register_service(text_of(self->id), text_of(name), service);
    return SPIRATION_OK;
}

void* SPIRATION_CALL host_query_service(const spiration_extension_desc* self,
                                        spiration_str_view extension_id,
                                        spiration_str_view name) {
    SPIRATION_UNUSED(self);
    extension_manager* mgr = manager();
    if (mgr == nullptr) return NULL;
    return mgr->get_service(text_of(extension_id), text_of(name));
}

/* ---- 八、拓展注册表 ---- */

spiration_extension_desc* SPIRATION_CALL host_find_extension(
    const spiration_extension_desc* self, spiration_str_view id) {
    SPIRATION_UNUSED(self);
    const std::string target = text_of(id);

    auto it = g_state().descriptors.find(target);
    if (it != g_state().descriptors.end()) return it->second;

    extension_manager* mgr = manager();
    if (mgr == nullptr) return NULL;
    extension* found = mgr->find_extension(target);
    if (found == nullptr) return NULL;
    return const_cast<spiration_extension_desc*>(shim(found));
}

size_t SPIRATION_CALL host_enumerate_extensions(
    const spiration_extension_desc* self,
    const spiration_extension_desc** out, size_t capacity) {
    SPIRATION_UNUSED(self);
    extension_manager* mgr = manager();
    if (mgr == nullptr) return 0u;

    std::vector<const spiration_extension_desc*> all;
    for (extension* ext : mgr->extensions()) {
        if (ext == nullptr) continue;

        const std::string id = ext->id();
        auto it = g_state().descriptors.find(id);
        if (it != g_state().descriptors.end()) {
            all.push_back(it->second);
        } else {
            all.push_back(shim(ext));
        }
    }

    if (out != NULL) {
        const size_t count = all.size() < capacity ? all.size() : capacity;
        for (size_t i = 0; i < count; ++i) out[i] = all[i];
    }
    return all.size();
}

/* ---- 九、可选接口 ---- */

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

/* ---- 十、加载器 ---- */

spiration_status SPIRATION_CALL host_register_loader(
    const spiration_extension_desc* self, const spiration_loader_api* loader) {
    if (self == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_loader: null descriptor");
    }
    return register_loader(text_of(self->id), loader);
}

const spiration_loader_api* SPIRATION_CALL host_query_loader(
    const spiration_extension_desc* self, spiration_str_view loader_id) {
    SPIRATION_UNUSED(self);
    return find_loader(text_of(loader_id));
}

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

} // namespace

const spiration_host_api* host_api() {
    /* 首次调用发生在宿主主线程（拓展装载期间），顺带固定主线程 ID。 */
    host_thread();
    static const spiration_host_api api = make_host_api();
    return &api;
}

// ---------------------------------------------------------------------------
// 描述符登记与垫片
// ---------------------------------------------------------------------------

void track(spiration_extension_desc* desc) {
    if (desc == nullptr || desc->id.data == nullptr) return;
    const std::string id = text_of(desc->id);
    if (id.empty()) return;

    g_state().descriptors[id] = desc;
    {
        std::lock_guard<std::mutex> lock(g_mutex());
        g_state().alive[desc] = std::make_shared<std::atomic<bool>>(true);
    }
}

void forget(const spiration_extension_desc* desc) {
    if (desc == nullptr) return;

    extension_manager* mgr = manager();
    auto subscriptions = g_state().subscriptions.find(desc);
    if (subscriptions != g_state().subscriptions.end()) {
        if (mgr != nullptr) {
            for (int64_t id : subscriptions->second) {
                if (id > 0) mgr->off_event(static_cast<int>(id));
            }
        }
        g_state().subscriptions.erase(subscriptions);
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex());
        auto it = g_state().alive.find(desc);
        if (it != g_state().alive.end()) {
            /* 尚未执行的 dispatch 任务据此丢弃。 */
            if (it->second) it->second->store(false);
            g_state().alive.erase(it);
        }
    }

    for (auto it = g_state().descriptors.begin();
         it != g_state().descriptors.end(); ++it) {
        if (it->second == desc) {
            g_state().descriptors.erase(it);
            break;
        }
    }

    /* 关闭该拓展打开的标签页、移除其菜单项：否则用户此后点击这些界面
       元素会跳转进已卸载的动态库。 */
    release_ui_owned(desc);
}

const spiration_extension_desc* shim(extension* ext) {
    if (ext == nullptr) return nullptr;

    const std::string id = ext->id();
    auto it = g_state().shims.find(id);
    if (it != g_state().shims.end()) return &it->second->desc;

    std::unique_ptr<shim_holder> holder(new shim_holder());
    holder->id = id;
    holder->name = ext->name().value_or(id);
    holder->version = ext->version();
    holder->description = ext->description().value_or(std::string());

    std::memset(&holder->desc, 0, sizeof(holder->desc));
    holder->desc.version = SPIRATION_STRUCT_VERSION;
    holder->desc.size = static_cast<uint32_t>(sizeof(spiration_extension_desc));
    holder->desc.phase = to_capi_phase(ext->phase());
    holder->desc.api_version_required = SPIRATION_API_VERSION;
    holder->desc.id = view_of(holder->id);
    holder->desc.name = view_of(holder->name);
    holder->desc.version_string = view_of(holder->version);
    holder->desc.description = view_of(holder->description);
    /* 垫片只有身份有效：回调恒为空，宿主函数表可直接复用。 */
    holder->desc.host = host_api();
    holder->desc.user_data = nullptr;
    holder->desc.on_initialize = nullptr;
    holder->desc.on_shutdown = nullptr;
    holder->desc.on_event = nullptr;

    const spiration_extension_desc* result = &holder->desc;
    g_state().shims[id] = std::move(holder);
    return result;
}

// ---------------------------------------------------------------------------
// 加载器注册表
// ---------------------------------------------------------------------------

spiration_status register_loader(const std::string& loader_id,
                                 const spiration_loader_api* loader) {
    if (loader_id.empty() || loader == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_loader: null argument");
    }
    if (loader->instantiate == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_loader: loader has no instantiate");
    }
    if (!SPIRATION_API_VERSION_COMPATIBLE(host_api()->version,
                                          loader->version)) {
        return fail(SPIRATION_ERR_UNSUPPORTED,
                    "register_loader: incompatible loader version");
    }

    g_state().loaders[loader_id] = loader;

    if (std::shared_ptr<logging::logger> out = logger()) {
        out->log({logging::level::info,
                  "loader \"" + loader_id + "\" registered", "capi"});
    }
    return SPIRATION_OK;
}

const spiration_loader_api* find_loader(const std::string& loader_id) {
    auto it = g_state().loaders.find(loader_id);
    return it == g_state().loaders.end() ? nullptr : it->second;
}

namespace {

/**
 * @brief 以 C 描述符为后端实现的 C++ 拓展实例。
 *
 * 该适配器让 C 拓展与脚本拓展可以直接接入 `extension_manager` 的既有流程
 * （阶段初始化、依赖排序、卸载、事件广播等）。
 */
class descriptor_extension : public spiration::extension {
public:
    descriptor_extension(const manifest_data& manifest,
                         spiration_extension_desc* desc,
                         const spiration_loader_api* loader)
        : spiration::extension(manifest), desc_(desc), loader_(loader) {}

    ~descriptor_extension() override {
        /* 由加载器创建的描述符在此归还。 */
        if (desc_ != nullptr && loader_ != nullptr &&
            loader_->release != nullptr) {
            loader_->release(desc_);
        }
    }

    tl::optional<std::string> name() const override {
        if (desc_ != nullptr && desc_->name.length > 0u) {
            return text_of(desc_->name);
        }
        return tl::nullopt;
    }

    tl::optional<std::string> description() const override {
        if (desc_ != nullptr && desc_->description.length > 0u) {
            return text_of(desc_->description);
        }
        return tl::nullopt;
    }

    init_phase phase() const override {
        return desc_ != nullptr ? from_capi_phase(desc_->phase)
                                : init_phase::normal;
    }

    bool initialize() override {
        if (desc_ == nullptr || desc_->on_initialize == nullptr) return false;
        result_scope scope;
        try {
            return desc_->on_initialize(desc_) == SPIRATION_TRUE;
        } catch (...) {
            /* 异常不得越过 ABI。 */
            return false;
        }
    }

    void shutdown() override {
        if (desc_ == nullptr) return;

        {
            result_scope scope;
            if (desc_->on_shutdown != nullptr) {
                try {
                    desc_->on_shutdown(desc_);
                } catch (...) {
                    /* 静默吞掉，继续清理。 */
                }
            }
        }

        forget(desc_);
    }

private:
    spiration_extension_desc* desc_ = nullptr;
    const spiration_loader_api* loader_ = nullptr;
};

} // namespace

std::unique_ptr<extension> wrap(const manifest_data& manifest,
                                spiration_extension_desc* desc,
                                const spiration_loader_api* loader) {
    if (desc == nullptr) return nullptr;
    return std::unique_ptr<extension>(
        new descriptor_extension(manifest, desc, loader));
}

// ---------------------------------------------------------------------------
// C API 装载
// ---------------------------------------------------------------------------

namespace {

/** @brief 校验描述符。 */
bool validate(const spiration_extension_desc* desc, std::string* error) {
    if (desc == nullptr) {
        if (error) *error = "entry returned no descriptor";
        return false;
    }
    if (desc->version == 0u) {
        if (error) *error = "descriptor version is zero";
        return false;
    }
    if (!SPIRATION_API_VERSION_COMPATIBLE(host_api()->version,
                                          desc->api_version_required)) {
        if (error) *error = "incompatible api_version_required";
        return false;
    }
    if (desc->id.data == nullptr || desc->id.length == 0u) {
        if (error) *error = "descriptor id is empty";
        return false;
    }
    return true;
}

} // namespace

std::unique_ptr<extension> try_load(const manifest_data& manifest,
                                    const std::string& manifest_json,
                                    void* library, std::string* error) {
    const spiration_host_api* host = host_api();
    const spiration_str_view manifest_view =
        spiration_str_view_of(manifest_json.data(), manifest_json.size());
    extension_manager* mgr = manager();

    /* --- 路径一：清单声明了加载器 --- */
    if (manifest.loader.has_value() && !manifest.loader->empty()) {
        const spiration_loader_api* loader = find_loader(*manifest.loader);
        if (loader == nullptr || loader->instantiate == nullptr) {
            if (error) *error = "loader not registered: " + *manifest.loader;
            return nullptr;
        }
        if (mgr == nullptr) {
            if (error) *error = "extension manager unavailable";
            return nullptr;
        }

        const ghc::filesystem::path directory =
            mgr->extension_directory(manifest.id);
        const ghc::filesystem::path entry =
            directory / manifest.main.value_or(std::string());

        const spiration_str_view id_view = view_of(manifest.id);
        const ghc::filesystem::path directory_normalized =
            directory.lexically_normal();
        const ghc::filesystem::path entry_normalized = entry.lexically_normal();
        const spiration_str_view dir_view =
            view_of(directory_normalized.string());
        const spiration_str_view entry_view = view_of(entry_normalized.string());

        /* 装载不属于回调作用域，仅清理上一次残留。 */
        clear_results();
        spiration_extension_desc* desc = loader->instantiate(
            host, id_view, dir_view, entry_view, manifest_view);
        if (!validate(desc, error)) return nullptr;

        manifest_data effective = manifest;
        if (desc->version_string.length > 0u) {
            effective.version = text_of(desc->version_string);
        }

        track(desc);
        return wrap(effective, desc, loader);
    }

    /* --- 路径二：动态库导出 C API 入口符号 --- */
    if (library != nullptr) {
        void* symbol = find_symbol(library, SPIRATION_EXTENSION_ENTRY_SYMBOL);
        if (symbol == nullptr) return nullptr; /* 交由调用方回退旧 ABI */

        spiration_extension_entry_fn entry =
            reinterpret_cast<spiration_extension_entry_fn>(symbol);

        clear_results();
        spiration_extension_desc* desc = nullptr;
        try {
            desc = const_cast<spiration_extension_desc*>(
                entry(host, manifest_view));
        } catch (...) {
            if (error) *error = "entry threw an exception";
            return nullptr;
        }
        if (!validate(desc, error)) return nullptr;

        /* 入口已填写 host，此处兜底。 */
        if (desc->host == nullptr) desc->host = host;

        manifest_data effective = manifest;
        if (desc->version_string.length > 0u) {
            effective.version = text_of(desc->version_string);
        }

        track(desc);
        return wrap(effective, desc, nullptr);
    }

    if (error) *error = "no loader and no library handle";
    return nullptr;
}

} // namespace capi
} // namespace spiration
