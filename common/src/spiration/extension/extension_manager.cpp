/**
 * @file extension_manager.cpp
 * @brief 扩展管理器实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/extension_manager.h>
#include <spiration/extension/extension.h>
#include <spiration/extension/extension_api.h>
#include <spiration/extension/extension_loader.h>
#include <spiration/extension/extension_manifest.h>
#include <spiration/extension/builtin.h>
#include <ghc/filesystem.hpp>
#include <spiration/utils/platform.h>
#include <spiration/utils/io.h>
#include <fmt/format.h>
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
        application::instance()->logger()->log({logging::level::warning, "already initialized", "extension/manager"});
        return;
    }
    initialized_ = true;

    register_builtin(std::make_unique<agent::extension>());
    register_builtin(std::make_unique<edit::extension>());
    register_builtin(std::make_unique<i18n::extension>());
    register_builtin(std::make_unique<theme::extension>());

    application::instance()->logger()->log({logging::level::info, "initialized", "extension/manager"});
}

void extension_manager::shutdown() {
    shutdown_all();
    extensions_.clear();
    initialized_ = false;
    application::instance()->logger()->log({logging::level::info, "shut down", "extension/manager"});
}

bool extension_manager::load_extension(const std::string& path) {
    if (!initialized_) {
        return false;
    }

    auto ext = extension_loader::load_extension_from(path);
    if (!ext.instance) {
        application::instance()->logger()->log({logging::level::error, fmt::format("failed to load extension from \"{}\"", path), "extension/manager"});
        return false;
    }

    for (const auto& existing : extensions_) {
        if (existing.instance && existing.instance->id() == ext.instance->id()) {
            application::instance()->logger()->log({logging::level::warning, fmt::format("extension \"{}\" already loaded", ext.instance->id()), "extension/manager"});
            return false;
        }
    }

    loaded_extension le;
    le.handle = std::move(ext.handle);
    le.instance = std::unique_ptr<extension>(ext.instance);
    le.initialized = false;
    extensions_.push_back(std::move(le));

    application::instance()->logger()->log({logging::level::info, fmt::format("loaded \"{}\" ({})", ext.instance->name(), ext.instance->version()), "extension/manager"});
    return true;
}

size_t extension_manager::load_extensions_from(const ghc::filesystem::path& directory) {
    if (!ghc::filesystem::exists(directory)) {
        application::instance()->logger()->log({logging::level::info, fmt::format("directory \"{}\" does not exist", directory.string()), "extension/manager"});
        return 0;
    }

    struct ext_info {
        ghc::filesystem::path dir_path;
        manifest_data manifest;
        bool has_manifest = false;
    };
    std::vector<ext_info> infos;

    for (const auto& entry : ghc::filesystem::directory_iterator(directory)) {
        if (!ghc::filesystem::exists(entry.path()) || !ghc::filesystem::is_directory(entry.path())) continue;

        ext_info info;
        info.dir_path = entry.path();

        ghc::filesystem::path manifest_path = entry.path() / "extension.json";
        if (!ghc::filesystem::exists(manifest_path) || ghc::filesystem::is_directory(manifest_path)) {
            continue;
        }
        std::string json = io::read_file_content(manifest_path);
        if (!json.empty()) {
            auto m = parse_extension_manifest(json);
            if (m) {
                if (m->api_version != 0 &&
                    m->api_version != kSupportedExtensionApiVersion) {
                    application::instance()->logger()->log({logging::level::warning, fmt::format("skip \"{}\": unsupported api_version={} (supported={})", m->id, m->api_version, kSupportedExtensionApiVersion), "extension/manager"});
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
                for (const auto& dep_entry : info.manifest.depends) {
                    const auto& dep_id = dep_entry.first;
                    const auto& constraint = dep_entry.second;
                    if (loaded_ids.find(dep_id) == loaded_ids.end()) {
                        deps_ok = false;
                        break;
                    }
                    // 依赖版本约束校验（约束为空表示不限定版本）。
                    if (constraint.empty()) continue;
                    extension* dep = find_extension(dep_id);
                    std::string dep_ver = dep ? dep->version() : "";
                    if (dep_ver.empty() || !version_matches(dep_ver, constraint)) {
                        application::instance()->logger()->log({logging::level::warning, fmt::format("dependency \"{}\" version \"{}\" does not satisfy \"{}\" for \"{}\"", dep_id, dep_ver, constraint, info.manifest.id), "extension/manager"});
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
                            application::instance()->logger()->log({logging::level::warning, fmt::format("id mismatch in \"{}\": manifest=\"{}\", instance=\"{}\"", info.dir_path.string(), info.manifest.id, result.instance->id()), "extension/manager"});
                        }
                        for (const auto& existing : extensions_) {
                            if (existing.instance &&
                                existing.instance->id() == result.instance->id()) {
                                application::instance()->logger()->log({logging::level::warning, fmt::format("extension \"{}\" already loaded", result.instance->id()), "extension/manager"});
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
                    application::instance()->logger()->log({logging::level::info, fmt::format("loaded \"{}\"", info.has_manifest ? info.manifest.name : info.dir_path.string()), "extension/manager"});
                }
            }
        }
    }

    for (size_t i = 0; i < infos.size(); ++i) {
        if (!loaded[i] && infos[i].has_manifest) {
            application::instance()->logger()->log({logging::level::warning, fmt::format("skipped \"{}\" (unmet dependencies)", infos[i].manifest.id), "extension/manager"});
        }
    }

    application::instance()->logger()->log({logging::level::info, fmt::format("loaded {} extension(s) from \"{}\"", count, directory.string()), "extension/manager"});
    return count;
}

bool extension_manager::unload_extension(const std::string& id) {
    for (auto it = extensions_.begin(); it != extensions_.end(); ++it) {
        if (it->instance && it->instance->id() == id) {
            if (it->initialized) {
                it->instance->shutdown();
            }
            application::instance()->logger()->log({logging::level::info, fmt::format("unloaded \"{}\"", id), "extension/manager"});
            unregister_services(id);
            extensions_.erase(it);
            return true;
        }
    }
    application::instance()->logger()->log({logging::level::warning, fmt::format("extension \"{}\" not found", id), "extension/manager"});
    return false;
}

size_t extension_manager::initialize_phase(init_phase phase) {
    size_t count = 0;
    for (auto& le : extensions_) {
        if (!le.initialized && le.instance && le.instance->phase() == phase) {
            le.instance->set_api(std::make_unique<spiration::extension_api>(le.instance->id()));
            if (le.instance->initialize()) {
                le.initialized = true;
                ++count;
                application::instance()->logger()->log({logging::level::info, fmt::format("initialized \"{}\" [{}]", le.instance->name(), phase == init_phase::early ? "early" : "normal"), "extension/manager"});
            } else {
                application::instance()->logger()->log({logging::level::error, fmt::format("failed to initialize \"{}\"", le.instance->name()), "extension/manager"});
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
    application::instance()->logger()->log({logging::level::info, "all extensions shut down", "extension/manager"});
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

ghc::filesystem::path extension_manager::extension_directory(const std::string& id) {
    for (const auto& le : extensions_) {
        if (le.instance && le.instance->id() == id) {
            if (!le.dir_path.empty() && ghc::filesystem::exists(le.dir_path))
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
            application::instance()->logger()->log({logging::level::warning, fmt::format("builtin extension \"{}\" already loaded", ext->id()), "extension/manager"});
            return;
        }
    }

    loaded_extension le;
    le.handle = extension_loader::lib_handle(nullptr);
    std::string ext_id = ext->id();
    le.instance = std::move(ext);
    le.initialized = false;
    le.dir_path = platform::data_directory() / ext_id;
    extensions_.push_back(std::move(le));

    application::instance()->logger()->log({logging::level::info, fmt::format("registered builtin \"{}\"", extensions_.back().instance->name()), "extension"});
}

int extension_manager::on_event(const std::string& event,
                                 std::function<void(const std::string&)> callback) {
    int id = next_subscription_id_++;
    events_[event][id] = std::move(callback);
    return id;
}

void extension_manager::off_event(int subscription_id) {
    for (auto& event_entry : events_) {
        auto& subs = event_entry.second;
        auto it = subs.find(subscription_id);
        if (it != subs.end()) {
            subs.erase(it);
            if (subs.empty()) {
                events_.erase(event_entry.first);
            }
            return;
        }
    }
}

void extension_manager::emit_event(const std::string& event, const std::string& data) {
    auto it = events_.find(event);
    if (it == events_.end()) return;

    auto callbacks = it->second;
    for (const auto& cb_entry : callbacks) {
        if (cb_entry.second) cb_entry.second(data);
    }
}

void extension_manager::register_service(const std::string& ext_id,
                                          const std::string& name, void* ptr) {
    services_[ext_id][name] = ptr;
    application::instance()->logger()->log({logging::level::info, fmt::format("\"{}\" registered \"{}\"", ext_id, name), "extension/service"});
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
