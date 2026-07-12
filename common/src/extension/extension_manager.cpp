/**
 * @file extension_manager.cpp
 * @brief 扩展管理器实现。
 * @author clk
 */

#include <extension/extension_manager.h>
#include <extension/extension.h>
#include <extension/extension_api.h>
#include <extension/extension_loader.h>
#include <utils/console.h>
#include <utils/platform.h>

#include <algorithm>

namespace spiration {

std::vector<extension_manager::loaded_extension> extension_manager::s_extensions;
std::shared_ptr<extension_api> extension_manager::s_api;
bool extension_manager::s_initialized = false;

void extension_manager::initialize(std::shared_ptr<extension_api> api) {
    if (s_initialized) {
        console::warning("extension_manager: already initialized");
        return;
    }
    s_api = std::move(api);
    s_initialized = true;
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
    size_t count = 0;

    for (const auto& entry : entries) {
        std::string ext_path = platform::join_path(directory, entry);

#ifdef _WIN32
        if (entry.size() < 4 || entry.substr(entry.size() - 4) != ".dll")
            continue;
#else
        if (entry.size() < 3 || entry.substr(entry.size() - 3) != ".so")
            continue;
#endif

        if (load_extension(ext_path)) {
            ++count;
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

} // namespace spiration
