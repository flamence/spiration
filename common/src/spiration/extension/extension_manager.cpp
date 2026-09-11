/**
 * @file extension_manager.cpp
 * @brief 扩展管理器实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/extension_manager.h>
#include <spiration/extension/extension.hpp>
#include <spiration/extension/extension_api.h>
#include <spiration/extension/extension_manifest.h>
#include <spiration/extension/builtin.h>
#include <ghc/filesystem.hpp>
#include <spiration/utils/platform.h>
#include <spiration/utils/io.h>
#include <fmt/format.h>
#include <map>
#include <memory>
#include <set>
#include <tl/optional.hpp>
#include <utility>

namespace spiration {

using extension_initialize_function = spiration::extension* (*)(spiration::manifest_data);

const std::string extension_manager::extension_manifest_name_ = "extension.json";
const std::string extension_manager::extension_initialize_symbol_name_ = "spiration_initialize";
const std::string extension_manager::extension_shutdown_symbol_name_ = "spiration_shutdown";

extension_manager::extension_manager(const application& application) : 
        application_(application),
        logger_(*application.logger()) {
    register_builtin(std::make_unique<agent::extension>());
    register_builtin(std::make_unique<edit::extension>());
    register_builtin(std::make_unique<i18n::extension>());
    register_builtin(std::make_unique<theme::extension>());
}

void extension_manager::shutdown() {
    for (std::pair<const std::string, loaded_extension>& entry : extensions_) {
        if (entry.second.initialized && entry.second.instance) {
            try {
                entry.second.instance->shutdown();
            } catch (...) {
                logger_.log({
                    logging::level::warning,
                    fmt::format("shutdown threw for \"{}\"", entry.second.instance->id()),
                    "extension/manager"
                });
            }
            entry.second.initialized = false;
        }
    }
    services_.clear();
    extensions_.clear();
}

bool extension_manager::load(const ghc::filesystem::path& path) {
    ghc::filesystem::path manifest_path = path / extension_manifest_name_;
    std::string manifest_json = io::read_file_content(manifest_path);
    tl::optional<manifest_data> manifest = parse_extension_manifest(manifest_json);

    if (!manifest.has_value() || !manifest->main.has_value()) {
        logger_.log({
            logging::level::trace,
            fmt::format("skipped extension by invaild manifest: {}", path.string()),
            "extension/manager"
        });
        return false;
    }

    loaded_extension entry = loaded_extension({
        .path=path
    });

    library_ptr handle = load_library(path / manifest->main.value());
    if (!handle || handle->mod == nullptr) {
        logger_.log({
            logging::level::trace,
            fmt::format("failed to load library: {}", path.string()),
            "extension/manager"
        });
        return false;
    }

    entry.handle = std::move(handle);
    extension_initialize_function symbol = reinterpret_cast<extension_initialize_function>(find_symbol(entry.handle.get(), extension_initialize_symbol_name_));
    if (symbol == nullptr) {
        logger_.log({
            logging::level::trace,
            fmt::format("skipped extension by undiscovered symbol: {}", path.string()),
            "extension/manager"
        });
        return false;
    }

    extension* extension = nullptr;
    try {
        extension = symbol(manifest.value());
    } catch (...) {
        logger_.log({
            logging::level::trace,
            fmt::format("bad extension: {}", path.string()),
            "extension/manager"
        });
        return false;
    }

    if (extension == nullptr) {
        logger_.log({
            logging::level::trace,
            fmt::format("returns an empty extension instance: {}", path.string()),
            "extension/manager"
        });
        return false;
    }

    entry.instance = std::unique_ptr<spiration::extension>(extension);

    if (extensions_.find(entry.instance->id()) != extensions_.end()) {
        logger_.log({
            logging::level::warning,
            fmt::format("there's already a extension with id \"{}\": {}", entry.instance->id(), path.string()),
            "extension/manager"
        });
        return false;
    }

    extensions_[entry.instance->id()] = std::move(entry);

    logger_.log({
        logging::level::trace,
        fmt::format("loaded {} v{} ({}) in {}", extension->name().value_or(extension->id()), extension->version(), extension->id(), path.string()),
        "extension/manager"
    });

    return true;
}

size_t extension_manager::load_from(const ghc::filesystem::path& directory) {
    if (!ghc::filesystem::exists(directory)) {
        application::instance()->logger()->log({
            logging::level::trace,
            fmt::format("wanna search extensions but does not exist: {}", directory.string()),
            "extension/manager"
        });
        return 0;
    }

    struct extension_info {
        ghc::filesystem::path dir_path;
        manifest_data manifest;
    };

    std::vector<extension_info> infos;

    for (const auto& entry : ghc::filesystem::directory_iterator(directory)) {
        if (!ghc::filesystem::is_directory(entry.path())) continue;

        ghc::filesystem::path manifest_path = entry.path() / extension_manifest_name_;
        if (!ghc::filesystem::exists(manifest_path) || ghc::filesystem::is_directory(manifest_path)) {
            continue;
        }

        std::string json = io::read_file_content(manifest_path);
        auto manifest = parse_extension_manifest(json);
        if (!manifest) continue;

        if (manifest->api_version != 0 &&
            manifest->api_version != kSupportedExtensionApiVersion) {
            application::instance()->logger()->log({
                logging::level::warning,
                fmt::format("skip \"{}\": unsupported api_version={} (supported={})",
                            manifest->id, manifest->api_version, kSupportedExtensionApiVersion),
                "extension/manager"
            });
            continue;
        }

        extension_info info;
        info.dir_path = entry.path();
        info.manifest = std::move(*manifest);
        infos.push_back(std::move(info));
    }

    std::set<std::string> loaded_ids;
    for (const auto& ext : extensions_) {
        if (ext.second.instance) {
            loaded_ids.insert(ext.second.instance->id());
        }
    }

    size_t count = 0;
    std::vector<bool> handled(infos.size(), false);
    bool progress = true;

    while (progress) {
        progress = false;
        for (size_t i = 0; i < infos.size(); ++i) {
            if (handled[i]) continue;

            const auto& info = infos[i];

            if (loaded_ids.find(info.manifest.id) != loaded_ids.end()) {
                handled[i] = true;
                continue;
            }

            bool deps_ok = true;
            for (const auto& dep_entry : info.manifest.depends) {
                const auto& dep_id = dep_entry.first;
                const auto& constraint = dep_entry.second;

                if (loaded_ids.find(dep_id) == loaded_ids.end()) {
                    deps_ok = false;
                    break;
                }

                if (constraint.empty()) continue;

                extension* dep = find_extension(dep_id);
                if (!dep) {
                    deps_ok = false;
                    break;
                }

                std::string dep_ver = dep->version();
                if (dep_ver.empty() || !version_matches(dep_ver, constraint)) {
                    logger_.log({
                        logging::level::trace,
                        fmt::format("dependency \"{}\" version \"{}\" does not satisfy \"{}\" for \"{}\"",
                                    dep_id, dep_ver, constraint, info.manifest.id),
                        "extension/manager"
                    });
                    deps_ok = false;
                    break;
                }
            }

            if (!deps_ok) {
                continue;
            }

            bool loaded_ok = load(info.dir_path);
            handled[i] = true;

            if (loaded_ok) {
                ++count;
                loaded_ids.insert(info.manifest.id);
                progress = true;
            } else {
                logger_.log({
                    logging::level::warning,
                    fmt::format("failed to load extension \"{}\" from {}",
                                info.manifest.id, info.dir_path.string()),
                    "extension/manager"
                });
            }
        }
    }

    for (size_t i = 0; i < infos.size(); ++i) {
        if (!handled[i]) {
            logger_.log({
                logging::level::warning,
                fmt::format("skipped \"{}\" (unmet dependencies)", infos[i].manifest.id),
                "extension/manager"
            });
        }
    }

    return count;
}

bool extension_manager::unload_extension(const std::string& id) {
    auto it = extensions_.find(id);
    if (it != extensions_.end()) {
        if (it->second.initialized) {
            try {
                it->second.instance->shutdown();
            } catch (...) {
                logger_.log({
                    logging::level::warning,
                    fmt::format("shutdown threw for \"{}\"", id),
                    "extension/manager"
                });
            }
        }
        application::instance()->logger()->log({
            logging::level::trace,
            fmt::format("unloaded \"{}\"", id),
            "extension/manager"
        });
        emit_event("extension:unloaded", id);
        unregister_services(id);
        extensions_.erase(it);
        return true;
    }
    logger_.log({
        logging::level::trace,
        fmt::format("cannot found extension \"{}\" when trying to unload it", id),
        "extension/manager"
    });
    return false;
}

size_t extension_manager::initialize(init_phase phase) {
    size_t count = 0;
    for (auto& le : extensions_) {
        if (!le.second.initialized && le.second.instance && le.second.instance->phase() == phase) {
            le.second.instance->set_api(std::make_unique<spiration::extension_api>(le.second.instance->id()));
            bool ok = false;
            try {
                ok = le.second.instance->initialize();
            } catch (...) {
                logger_.log({
                    logging::level::warning,
                    fmt::format("initialize threw for \"{}\"", le.second.instance->id()),
                    "extension/manager"
                });
            }
            if (ok) {
                le.second.initialized = true;
                ++count;
                application::instance()->logger()->log({
                    logging::level::info,
                    fmt::format("initialized \"{}\" [{}]",
                        le.second.instance->name().value_or(le.second.instance->id()),
                        phase == init_phase::early ? "early" : "normal"
                    ), 
                    "extension/manager"
                });
            } else {
                logger_.log({
                    logging::level::error,
                    fmt::format("failed to initialize \"{}\"", le.second.instance->name().value_or(le.second.instance->id())),
                    "extension/manager"
                });
            }
        }
    }
    return count;
}

std::vector<extension*> extension_manager::extensions() {
    std::vector<extension*> result;
    for (const auto& le : extensions_) {
        if (le.second.instance) {
            result.push_back(le.second.instance.get());
        }
    }
    return result;
}

extension* extension_manager::find_extension(const std::string& id) {
    for (const auto& le : extensions_) {
        if (le.second.instance && le.second.instance->id() == id) {
            return le.second.instance.get();
        }
    }
    return nullptr;
}

size_t extension_manager::count() {
    return extensions_.size();
}

ghc::filesystem::path extension_manager::extension_directory(const std::string& id) {
    return platform::data_directory() / id;
}

void extension_manager::register_builtin(std::unique_ptr<extension> ext) {
    if (!ext) return;

    for (const auto& existing : extensions_) {
        if (existing.second.instance && existing.second.instance->id() == ext->id()) {
            return;
        }
    }

    loaded_extension le;
    le.handle = library_ptr(nullptr);
    std::string ext_id = ext->id();
    le.instance = std::move(ext);
    le.initialized = false;
    le.path = platform::data_directory() / ext_id;
    extensions_[ext_id] = std::move(le);

    logger_.log({
        logging::level::trace,
        fmt::format("registered builtin \"{}\"", ext_id),
        "extension/manager"
    });
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
