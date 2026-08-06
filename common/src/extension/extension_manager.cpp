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
#include <memory>
#include <set>

namespace spiration {

std::vector<extension_manager::loaded_extension> extension_manager::extensions_;
bool extension_manager::initialized_ = false;
std::map<std::string, std::map<int, std::function<void(const std::string&)>>> extension_manager::events_;
int extension_manager::next_subscription_id_ = 1;
std::map<std::string, std::map<std::string, void*>> extension_manager::services_;

extension_manager& extension_manager::instance() {
    static extension_manager inst;
    return inst;
}

extension_manager::extension_manager() {
    if (initialized_) {
        console::warning("extension/manager", "already initialized");
        return;
    }
    initialized_ = true;

    register_builtin(std::make_unique<agent::extension>());
    register_builtin(std::make_unique<edit::extension>());
    register_builtin(std::make_unique<i18n::extension>());
    register_builtin(std::make_unique<theme::extension>());

    console::info("extension/manager", "initialized");
}

void extension_manager::shutdown() {
    shutdown_all();
    extensions_.clear();
    initialized_ = false;
    console::info("extension/manager", "shut down");
}

bool extension_manager::load_extension(const std::string& path) {
    if (!initialized_) {
        return false;
    }

    auto ext = extension_loader::load_extension_from(path);
    if (!ext.instance) {
        console::error("extension/manager", "failed to load extension from \"%s\"",
                       path.c_str());
        return false;
    }

    for (const auto& existing : extensions_) {
        if (existing.instance && existing.instance->id() == ext.instance->id()) {
            console::warning("extension/manager", "extension \"%s\" already loaded",
                             ext.instance->id().c_str());
            return false;
        }
    }

    loaded_extension le;
    le.handle = std::move(ext.handle);
    le.instance = std::unique_ptr<extension>(ext.instance);
    le.initialized = false;
    extensions_.push_back(std::move(le));

    console::info("extension/manager", "loaded \"%s\" (%s)",
                  ext.instance->name().c_str(),
                  ext.instance->version().c_str());
    return true;
}

size_t extension_manager::load_extensions_from(const std::string& directory) {
    if (!platform::file_exists(directory)) {
        console::info("extension/manager", "directory \"%s\" does not exist",
                      directory.c_str());
        return 0;
    }

    auto entries = platform::list_directory(directory);

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

        std::string manifest_path = platform::join_path(subdir, "extension.json");
        std::string json = extension_loader::read_file_text(manifest_path);
        if (!json.empty()) {
            auto m = parse_extension_manifest(json);
            if (m) {
                // api_version 校验：未声明（0）视为兼容，声明了非支持版本则拒绝。
                if (m->api_version != 0 &&
                    m->api_version != kSupportedExtensionApiVersion) {
                    console::warning(
                        "extension/manager",
                        "skip \"%s\": unsupported api_version=%d (supported=%d)",
                        m->id.c_str(), m->api_version,
                        kSupportedExtensionApiVersion);
                    continue;
                }
                info.manifest = std::move(*m);
                info.has_manifest = true;
            }
        }

        infos.push_back(std::move(info));
    }

    std::vector<ext_info> sorted;
    std::set<std::string> loaded_ids;
    std::set<std::string> remaining;

    for (const auto& info : infos) {
        std::string id = info.has_manifest ? info.manifest.id : "";
        if (!id.empty()) remaining.insert(id);
    }

    size_t count = 0;
    std::vector<bool> loaded(infos.size(), false);
    bool progress = true;

    while (progress) {
        progress = false;
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i]) continue;

            const auto& info = infos[i];

            bool deps_ok = true;
            if (info.has_manifest) {
                for (const auto& [dep_id, constraint] : info.manifest.depends) {
                    if (loaded_ids.find(dep_id) == loaded_ids.end()) {
                        deps_ok = false;
                        break;
                    }
                    // 依赖版本约束校验（约束为空表示不限定版本）。
                    if (constraint.empty()) continue;
                    extension* dep = find_extension(dep_id);
                    std::string dep_ver = dep ? dep->version() : "";
                    if (dep_ver.empty() || !version_matches(dep_ver, constraint)) {
                        console::warning(
                            "extension/manager",
                            "dependency \"%s\" version \"%s\" does not satisfy \"%s\" for \"%s\"",
                            dep_id.c_str(), dep_ver.c_str(),
                            constraint.c_str(), info.manifest.id.c_str());
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
                                "extension/manager",
                                "id mismatch in \"%s\": "
                                "manifest=\"%s\", instance=\"%s\"",
                                info.dir_path.c_str(),
                                info.manifest.id.c_str(),
                                result.instance->id().c_str());
                        }
                        for (const auto& existing : extensions_) {
                            if (existing.instance &&
                                existing.instance->id() == result.instance->id()) {
                                console::warning(
                                    "extension/manager",
                                    "extension \"%s\" already loaded",
                                    result.instance->id().c_str());
                                ok = false;
                                break;
                            }
                        }
                        if (ok || extensions_.empty() ||
                            extensions_.back().instance.get() != result.instance) {
                            loaded_extension le;
                            le.handle = std::move(result.handle);
                            le.instance = std::unique_ptr<extension>(result.instance);
                            le.initialized = false;
                            le.dir_path = info.dir_path;
                            extensions_.push_back(std::move(le));
                            ok = true;
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
                    console::info("extension/manager", "loaded \"%s\"",
                                  info.has_manifest ? info.manifest.name.c_str()
                                                    : info.dir_path.c_str());
                }
            }
        }
    }

    for (size_t i = 0; i < infos.size(); ++i) {
        if (!loaded[i] && infos[i].has_manifest) {
            console::warning("extension/manager", "skipped \"%s\" (unmet dependencies)",
                             infos[i].manifest.id.c_str());
        }
    }

    console::info("extension/manager", "loaded %zu extension(s) from \"%s\"",
                  count, directory.c_str());
    return count;
}

bool extension_manager::unload_extension(const std::string& id) {
    for (auto it = extensions_.begin(); it != extensions_.end(); ++it) {
        if (it->instance && it->instance->id() == id) {
            if (it->initialized) {
                it->instance->shutdown();
            }
            console::info("extension/manager", "unloaded \"%s\"", id.c_str());
            unregister_services(id);
            extensions_.erase(it);
            return true;
        }
    }
    console::warning("extension/manager", "extension \"%s\" not found", id.c_str());
    return false;
}

size_t extension_manager::initialize_all() {
    size_t count = 0;
    count += initialize_phase(init_phase::early);
    count += initialize_phase(init_phase::normal);
    return count;
}

size_t extension_manager::initialize_phase(init_phase phase) {
    size_t count = 0;
    for (auto& le : extensions_) {
        if (!le.initialized && le.instance && le.instance->phase() == phase) {
            le.instance->set_api(std::make_unique<spiration::extension_api>(le.instance->id()));
            if (le.instance->initialize()) {
                le.initialized = true;
                ++count;
                console::info("extension/manager", "initialized \"%s\" [%s]",
                              le.instance->name().c_str(),
                              phase == init_phase::early ? "early" : "normal");
            } else {
                console::error("extension/manager", "failed to initialize \"%s\"",
                               le.instance->name().c_str());
            }
        }
    }
    return count;
}

void extension_manager::shutdown_all() {
    for (auto& le : extensions_) {
        if (le.initialized && le.instance) {
            le.instance->shutdown();
            le.initialized = false;
        }
    }
    services_.clear();
    console::info("extension/manager", "all extensions shut down");
}

std::vector<extension*> extension_manager::extensions() {
    std::vector<extension*> result;
    for (const auto& le : extensions_) {
        if (le.instance) {
            result.push_back(le.instance.get());
        }
    }
    return result;
}

extension* extension_manager::find_extension(const std::string& id) {
    for (const auto& le : extensions_) {
        if (le.instance && le.instance->id() == id) {
            return le.instance.get();
        }
    }
    return nullptr;
}

size_t extension_manager::count() {
    return extensions_.size();
}

std::string extension_manager::extension_directory(const std::string& id) {
    for (const auto& le : extensions_) {
        if (le.instance && le.instance->id() == id) {
            if (!le.dir_path.empty() && platform::file_exists(le.dir_path))
                return le.dir_path;
            return id;
        }
    }
    return {};
}

void extension_manager::register_builtin(std::unique_ptr<extension> ext) {
    if (!ext) return;

    for (const auto& existing : extensions_) {
        if (existing.instance && existing.instance->id() == ext->id()) {
            console::warning("extension/manager", "builtin extension \"%s\" already loaded",
                             ext->id().c_str());
            return;
        }
    }

    loaded_extension le;
    le.handle = extension_loader::lib_handle(nullptr);
    std::string ext_id = ext->id();
    le.instance = std::move(ext);
    le.initialized = false;
    le.dir_path = platform::join_path(platform::extension_directory(), ext_id);
    extensions_.push_back(std::move(le));

    console::info("extension", "registered builtin \"%s\"", 
                  extensions_.back().instance->name().c_str());
}

int extension_manager::on_event(const std::string& event,
                                 std::function<void(const std::string&)> callback) {
    int id = next_subscription_id_++;
    events_[event][id] = std::move(callback);
    return id;
}

void extension_manager::off_event(int subscription_id) {
    for (auto& [event, subs] : events_) {
        auto it = subs.find(subscription_id);
        if (it != subs.end()) {
            subs.erase(it);
            if (subs.empty()) {
                events_.erase(event);
            }
            return;
        }
    }
}

void extension_manager::emit_event(const std::string& event, const std::string& data) {
    auto it = events_.find(event);
    if (it == events_.end()) return;

    auto callbacks = it->second;
    for (const auto& [id, cb] : callbacks) {
        if (cb) cb(data);
    }
}

void extension_manager::register_service(const std::string& ext_id,
                                          const std::string& name, void* ptr) {
    services_[ext_id][name] = ptr;
    console::info("extension/service", "\"%s\" registered \"%s\"", ext_id.c_str(), name.c_str());
}

void* extension_manager::get_service(const std::string& ext_id,
                                      const std::string& name) {
    auto ext_it = services_.find(ext_id);
    if (ext_it == services_.end()) return nullptr;
    auto svc_it = ext_it->second.find(name);
    return (svc_it != ext_it->second.end()) ? svc_it->second : nullptr;
}

void extension_manager::unregister_services(const std::string& ext_id) {
    services_.erase(ext_id);
}

} // namespace spiration
