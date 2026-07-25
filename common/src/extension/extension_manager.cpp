/**
 * @file extension_manager.cpp
 * @brief 扩展管理器实现。
 * @author clk
 */

#include <extension/extension_manager.h>
#include <extension/extension.h>
#include <extension/extension_api.h>
#include <extension/extension_loader.h>
#include <extension/extension_manifest.h>
#include <extension/builtin.h>
#include <utils/console.h>
#include <utils/platform.h>

#include <algorithm>
#include <map>
#include <set>

namespace spiration {

std::vector<extension_manager::loaded_extension> extension_manager::s_extensions;
std::shared_ptr<extension_api> extension_manager::s_api;
bool extension_manager::s_initialized = false;
std::map<std::string, std::map<int, std::function<void(const std::string&)>>> extension_manager::s_events;
int extension_manager::s_next_subscription_id = 1;

void extension_manager::initialize(std::shared_ptr<extension_api> api) {
    if (s_initialized) {
        console::warning("extension_manager: already initialized");
        return;
    }
    s_api = std::move(api);
    s_initialized = true;

    register_builtin(std::make_unique<edit::extension>());
    register_builtin(std::make_unique<i18n::extension>());
    register_builtin(std::make_unique<theme::extension>());

    console::info("extension_manager: initialized");
}

void extension_manager::shutdown() {
    shutdown_all();
    s_extensions.clear();
    s_api.reset();
    s_initialized = false;
    console::info("extension_manager: shut down");
}

bool extension_manager::load_extension(const std::string& path) {
    if (!s_initialized) {
        console::error("extension_manager: not initialized");
        return false;
    }

    auto ext = extension_loader::load_extension_from(path);
    if (!ext.instance) {
        console::error("extension_manager: failed to load extension from '%s'",
                       path.c_str());
        return false;
    }

    for (const auto& existing : s_extensions) {
        if (existing.instance && existing.instance->id() == ext.instance->id()) {
            console::warning("extension_manager: extension '%s' already loaded",
                             ext.instance->id().c_str());
            return false;
        }
    }

    loaded_extension le;
    le.handle = std::move(ext.handle);
    le.instance = ext.instance;
    le.initialized = false;
    s_extensions.push_back(std::move(le));

    console::info("extension_manager: loaded '%s' (%s)",
                  ext.instance->name().c_str(),
                  ext.instance->version().c_str());
    return true;
}

size_t extension_manager::load_extensions_from(const std::string& directory) {
    if (!platform::file_exists(directory)) {
        console::info("extension_manager: directory '%s' does not exist",
                      directory.c_str());
        return 0;
    }

    auto entries = platform::list_directory(directory);

    /** 第一遍：收集所有扩展目录和 manifest */
    struct ext_info {
        std::string dir_path;
        manifest_data manifest;
        bool has_manifest = false;
    };
    std::vector<ext_info> infos;

    for (const auto& entry : entries) {
        std::string subdir = platform::join_path(directory, entry);
        if (!platform::file_exists(platform::join_path(subdir, "."))) continue;

        ext_info info;
        info.dir_path = subdir;

        /** 尝试读取 extension.json */
        std::string manifest_path = platform::join_path(subdir, "extension.json");
        std::string json = extension_loader::read_file_text(manifest_path);
        if (!json.empty()) {
            auto m = parse_extension_manifest(json);
            if (m) {
                info.manifest = std::move(*m);
                info.has_manifest = true;
            }
        }

        infos.push_back(std::move(info));
    }

    /** 也扫描旧格式（根目录下的 .dll/.so） */
    for (const auto& entry : entries) {
        std::string full_path = platform::join_path(directory, entry);
        bool is_dll = false;
#ifdef _WIN32
        if (entry.size() >= 4 && entry.substr(entry.size() - 4) == ".dll") is_dll = true;
#else
        if (entry.size() >= 3 && entry.substr(entry.size() - 3) == ".so") is_dll = true;
#endif
        if (is_dll) {
            ext_info info;
            info.dir_path = directory;
            info.has_manifest = false;
            infos.push_back(std::move(info));
        }
    }

    /** 第二遍：拓扑排序（有 manifest 且声明了依赖的按顺序排） */
    std::vector<ext_info> sorted;
    std::set<std::string> loaded_ids;
    std::set<std::string> remaining;

    for (const auto& info : infos) {
        std::string id = info.has_manifest ? info.manifest.id : "";
        if (!id.empty()) remaining.insert(id);
    }

    /** 简单拓扑：迭代直到全部加载或无法继续 */
    size_t count = 0;
    std::vector<bool> loaded(infos.size(), false);
    bool progress = true;

    while (progress) {
        progress = false;
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i]) continue;

            const auto& info = infos[i];

            /** 检查依赖是否已满足 */
            bool deps_ok = true;
            if (info.has_manifest) {
                for (const auto& [dep_id, constraint] : info.manifest.depends) {
                    if (loaded_ids.find(dep_id) == loaded_ids.end()) {
                        deps_ok = false;
                        break;
                    }
                }
            }

            if (deps_ok) {
                bool ok = false;
                if (info.has_manifest) {
                    auto result = extension_loader::load_extension_from_dir(
                        info.dir_path, nullptr);
                    if (result.instance) {
                        /** 验证 id 一致 */
                        if (result.instance->id() != info.manifest.id) {
                            console::warning(
                                "extension_manager: id mismatch in '%s': "
                                "manifest='%s', instance='%s'",
                                info.dir_path.c_str(),
                                info.manifest.id.c_str(),
                                result.instance->id().c_str());
                        }
                        for (const auto& existing : s_extensions) {
                            if (existing.instance &&
                                existing.instance->id() == result.instance->id()) {
                                console::warning(
                                    "extension_manager: extension '%s' already loaded",
                                    result.instance->id().c_str());
                                ok = false;
                                break;
                            }
                        }
                        if (ok || s_extensions.empty() ||
                            s_extensions.back().instance != result.instance) {
                            loaded_extension le;
                            le.handle = std::move(result.handle);
                            le.instance = result.instance;
                            le.initialized = false;
                            s_extensions.push_back(std::move(le));
                            ok = true;
                        }
                    }
                } else {
                    /** 旧格式：直接加载目录中的 DLL */
                    auto dlls = platform::list_directory(info.dir_path);
                    for (const auto& dll : dlls) {
#ifdef _WIN32
                        if (dll.size() >= 4 && dll.substr(dll.size() - 4) == ".dll")
#else
                        if (dll.size() >= 3 && dll.substr(dll.size() - 3) == ".so")
#endif
                        {
                            std::string dll_path = platform::join_path(info.dir_path, dll);
                            ok = load_extension(dll_path);
                            if (ok) break;
                        }
                    }
                }

                if (ok) {
                    loaded[i] = true;
                    progress = true;
                    ++count;
                    if (info.has_manifest) {
                        loaded_ids.insert(info.manifest.id);
                    }
                    console::info("extension_manager: loaded '%s'",
                                  info.has_manifest ? info.manifest.name.c_str()
                                                    : info.dir_path.c_str());
                }
            }
        }
    }

    /** 报告被跳过的扩展 */
    for (size_t i = 0; i < infos.size(); ++i) {
        if (!loaded[i] && infos[i].has_manifest) {
            console::warning("extension_manager: skipped '%s' (unmet dependencies)",
                             infos[i].manifest.id.c_str());
        }
    }

    console::info("extension_manager: loaded %zu extension(s) from '%s'",
                  count, directory.c_str());
    return count;
}

bool extension_manager::unload_extension(const std::string& id) {
    for (auto it = s_extensions.begin(); it != s_extensions.end(); ++it) {
        if (it->instance && it->instance->id() == id) {
            if (it->initialized) {
                it->instance->shutdown();
            }
            console::info("extension_manager: unloaded '%s'", id.c_str());
            s_extensions.erase(it);
            return true;
        }
    }
    console::warning("extension_manager: extension '%s' not found", id.c_str());
    return false;
}

size_t extension_manager::initialize_all() {
    size_t count = 0;
    for (auto& le : s_extensions) {
        if (!le.initialized && le.instance) {
            le.instance->set_api(s_api.get());
            s_api->set_calling_extension(le.instance->id());
            if (le.instance->initialize()) {
                le.initialized = true;
                ++count;
                console::info("extension_manager: initialized '%s'",
                              le.instance->name().c_str());
            } else {
                console::error("extension_manager: failed to initialize '%s'",
                               le.instance->name().c_str());
            }
        }
    }
    s_api->set_calling_extension("");
    return count;
}

void extension_manager::shutdown_all() {
    for (auto& le : s_extensions) {
        if (le.initialized && le.instance) {
            le.instance->shutdown();
            le.initialized = false;
        }
    }
    console::info("extension_manager: all extensions shut down");
}

std::vector<extension*> extension_manager::extensions() {
    std::vector<extension*> result;
    for (const auto& le : s_extensions) {
        if (le.instance) {
            result.push_back(le.instance);
        }
    }
    return result;
}

extension* extension_manager::find_extension(const std::string& id) {
    for (const auto& le : s_extensions) {
        if (le.instance && le.instance->id() == id) {
            return le.instance;
        }
    }
    return nullptr;
}

size_t extension_manager::count() {
    return s_extensions.size();
}

void extension_manager::register_builtin(std::unique_ptr<extension> ext) {
    if (!ext) return;

    for (const auto& existing : s_extensions) {
        if (existing.instance && existing.instance->id() == ext->id()) {
            console::warning("extension_manager: builtin extension '%s' already loaded",
                             ext->id().c_str());
            return;
        }
    }

    loaded_extension le;
    le.handle = extension_loader::lib_handle(nullptr);
    le.instance = ext.release();
    le.initialized = false;
    s_extensions.push_back(std::move(le));

    console::info("extension_manager: registered builtin '%s'", 
                  s_extensions.back().instance->name().c_str());
}

int extension_manager::on_event(const std::string& event,
                                 std::function<void(const std::string&)> callback) {
    int id = s_next_subscription_id++;
    s_events[event][id] = std::move(callback);
    return id;
}

void extension_manager::off_event(int subscription_id) {
    for (auto& [event, subs] : s_events) {
        auto it = subs.find(subscription_id);
        if (it != subs.end()) {
            subs.erase(it);
            if (subs.empty()) {
                s_events.erase(event);
            }
            return;
        }
    }
}

void extension_manager::emit_event(const std::string& event, const std::string& data) {
    auto it = s_events.find(event);
    if (it == s_events.end()) return;

    /** 复制回调列表防止回调中修改订阅表 */
    auto callbacks = it->second;
    for (const auto& [id, cb] : callbacks) {
        if (cb) cb(data);
    }
}

} // namespace spiration
