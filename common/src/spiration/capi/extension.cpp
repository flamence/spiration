/**
 * @file extension.cpp
 * @brief 描述符登记、垫片描述符、C++ 拓展包装与 C API 装载路径。
 * @author clk
 *
 * 本文件是 C API 与既有 C++ 拓展体系之间的接缝：
 *
 * ```
 * spiration_extension_desc（C） ↔ spiration::extension（C++ 虚基类）
 * ```
 *
 * - `track` / `forget`：描述符的登记与注销（含订阅、待执行任务、界面资源）；
 * - `shim`：为原生 C++ 拓展伪造一份只有身份字段有效的描述符；
 * - `wrap`：把描述符包装成 `extension_manager` 能管理的 C++ 实例；
 * - `try_load`：两条装载路径（清单声明加载器 / 动态库导出入口符号）。
 */

#include "internal.h"

#include <spiration/extension/extension.hpp>
#include <spiration/extension/extension_manager.h>

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace spiration {
namespace capi {

namespace {

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

/**
 * @brief 原生 C++ 拓展的垫片描述符表。
 * @note 条目生命周期与进程一致；键为拓展 ID。
 */
std::map<std::string, std::unique_ptr<shim_holder>>& shims() {
    static std::map<std::string, std::unique_ptr<shim_holder>> table;
    return table;
}

/* -------------------------------------------------------------------------
 * 符号解析
 * ---------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------
 * 描述符登记与垫片
 * ---------------------------------------------------------------------- */

void track(spiration_extension_desc* desc) {
    if (desc == nullptr || desc->id.data == nullptr) return;
    const std::string id = text_of(desc->id);
    if (id.empty()) return;

    global_state().descriptors[id] = desc;
    {
        std::lock_guard<std::mutex> lock(state_mutex());
        global_state().alive[desc] = std::make_shared<std::atomic<bool>>(true);
    }
}

void forget(const spiration_extension_desc* desc) {
    if (desc == nullptr) return;

    extension_manager* mgr = manager();
    auto subscriptions = global_state().subscriptions.find(desc);
    if (subscriptions != global_state().subscriptions.end()) {
        if (mgr != nullptr) {
            for (int64_t id : subscriptions->second) {
                if (id > 0) mgr->off_event(static_cast<int>(id));
            }
        }
        global_state().subscriptions.erase(subscriptions);
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex());
        auto it = global_state().alive.find(desc);
        if (it != global_state().alive.end()) {
            /* 尚未执行的 dispatch 任务据此丢弃。 */
            if (it->second) it->second->store(false);
            global_state().alive.erase(it);
        }
    }

    for (auto it = global_state().descriptors.begin();
         it != global_state().descriptors.end(); ++it) {
        if (it->second == desc) {
            global_state().descriptors.erase(it);
            break;
        }
    }

    /* 关闭该拓展打开的标签页、移除其菜单项：否则用户此后点击这些界面
       元素会跳转进已卸载的动态库。 */
    release_ui_owned(desc);

    /* 注销该拓展遗留的智能体 provider；`user_data` 由拓展自行释放。 */
    release_agent_owned(desc);
}

const spiration_extension_desc* shim(extension* ext) {
    if (ext == nullptr) return nullptr;

    const std::string id = ext->id();
    auto it = shims().find(id);
    if (it != shims().end()) return &it->second->desc;

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
    shims()[id] = std::move(holder);
    return result;
}

spiration_extension_desc* shim_descriptor(const std::string& id,
                                          const std::string& version) {
    if (id.empty()) return nullptr;

    auto it = shims().find(id);
    if (it != shims().end()) {
        /* 已登记：仅刷新版本，因为描述符字符串借用 `shim_holder` 的存储。 */
        shim_holder& holder = *it->second;
        holder.version = version;
        holder.desc.version_string = view_of(holder.version);
        return &holder.desc;
    }

    std::unique_ptr<shim_holder> holder(new shim_holder());
    holder->id = id;
    holder->name = id;
    holder->version = version;
    holder->description.clear();

    std::memset(&holder->desc, 0, sizeof(holder->desc));
    holder->desc.version = SPIRATION_STRUCT_VERSION;
    holder->desc.size = static_cast<uint32_t>(sizeof(spiration_extension_desc));
    holder->desc.phase = SPIRATION_PHASE_NORMAL;
    holder->desc.api_version_required = SPIRATION_API_VERSION;
    holder->desc.id = view_of(holder->id);
    holder->desc.name = view_of(holder->name);
    holder->desc.version_string = view_of(holder->version);
    holder->desc.description = view_of(holder->description);
    /* 描述符已带宿主函数表，故装载期即可 `host->log(self, ...)`。 */
    holder->desc.host = host_api();
    holder->desc.user_data = nullptr;
    holder->desc.on_initialize = nullptr;
    holder->desc.on_shutdown = nullptr;
    holder->desc.on_event = nullptr;

    spiration_extension_desc* result = &holder->desc;
    shims()[id] = std::move(holder);
    return result;
}

/* -------------------------------------------------------------------------
 * C++ 拓展包装
 * ---------------------------------------------------------------------- */

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

    ~descriptor_extension() override { release_descriptor(); }

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
        release_descriptor();
    }

private:
    void release_descriptor() {
        if (desc_ == nullptr) return;
        if (loader_ != nullptr && loader_->release != nullptr) {
            loader_->release(desc_);
        }
        desc_ = nullptr;
    }

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

/* -------------------------------------------------------------------------
 * C API 装载
 * ---------------------------------------------------------------------- */

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

        /* 注意：`view_of` 返回的视图**借用**入参字符串，因此必须先把
           `.string()` 的结果落到具名变量里。直接写
           `view_of(path.string())` 会让视图指向该行结束即析构的临时对象，
           等到 `instantiate` 读取时已是失效的栈内存（表现为路径乱码）。 */
        const std::string directory_text = directory.lexically_normal().string();
        const std::string entry_text = entry.lexically_normal().string();

        const spiration_str_view id_view = view_of(manifest.id);
        const spiration_str_view dir_view = view_of(directory_text);
        const spiration_str_view entry_view = view_of(entry_text);

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
