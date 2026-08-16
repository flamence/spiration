/**
 * @file extension_loader.cpp
 * @brief 拓展加载器实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/extension_loader.h>
#include <spiration/extension/extension.h>

#include <dlfcn.h>

#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace spiration {

void extension_loader::library_deleter::operator()(
    library_handle* handle) const {
    if (!handle) return;
    if (handle->mod) dlclose(handle->mod);
    delete handle;
}

extension_loader::lib_handle extension_loader::load_library(const std::string& path) {
    auto* handle = new library_handle();
    lib_handle result(handle);

    handle->mod = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle->mod) {
        application::instance()->logger()->log({ .level = logging::level::error, .message = std::format("{}", dlerror()), .tag = "extension/loader" });
        return lib_handle(nullptr);
    }

    application::instance()->logger()->log({ .level = logging::level::info, .message = std::format("loaded library \"{}\"", path), .tag = "extension/loader" });
    return result;
}

void* extension_loader::find_symbol(library_handle* handle, const std::string& symbol_name) {
    if (!handle || !handle->mod) {
        application::instance()->logger()->log({ .level = logging::level::error, .message = "invalid library handle", .tag = "extension/loader" });
        return nullptr;
    }

    void* ptr = dlsym(handle->mod, symbol_name.c_str());
    if (!ptr) {
        application::instance()->logger()->log({ .level = logging::level::error, .message = std::format("{}", dlerror()), .tag = "extension/loader" });
    }

    return ptr;
}

extension_loader::load_result extension_loader::load_extension_from(const std::string& path) {
    load_result result;

    auto handle = load_library(path);
    if (!handle) return result;

    auto create_fn = reinterpret_cast<extension_create_func>(
        find_symbol(handle.get(), "create_extension"));
    if (!create_fn) {
        application::instance()->logger()->log({ .level = logging::level::error, .message = std::format("no \"create_extension\" symbol in \"{}\"", path), .tag = "extension/loader" });
        return result;
    }

    extension* ext = create_fn();
    if (!ext) {
        application::instance()->logger()->log({ .level = logging::level::error, .message = std::format("create_extension() returned null from \"{}\"", path), .tag = "extension/loader" });
        return result;
    }

    result.handle = std::move(handle);
    result.instance = ext;

    application::instance()->logger()->log({ .level = logging::level::info, .message = std::format("loaded extension \"{}\" v{} from \"{}\"", ext->name(), ext->version(), path), .tag = "extension/loader" });
    return result;
}

std::string extension_loader::read_file_text(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

extension_loader::load_result extension_loader::load_extension_from_dir(
    const std::string& dir_path, manifest_data* out_manifest) {
    load_result result;

    std::string manifest_path = platform::join_path(dir_path, "extension.json");
    std::string manifest_json = read_file_text(manifest_path);

    if (manifest_json.empty()) {
        auto entries = platform::list_directory(dir_path);
        for (const auto& entry : entries) {
            if (entry.size() >= 3 && entry.substr(entry.size() - 3) == ".so") {
                std::string dll_path = platform::join_path(dir_path, entry);
                result = load_extension_from(dll_path);
                if (result.instance) return result;
            }
        }
        application::instance()->logger()->log({ .level = logging::level::error, .message = std::format("no extension.json or .so found in '{}'", dir_path), .tag = "extension/loader" });
        return result;
    }

    auto manifest = parse_extension_manifest(manifest_json);
    if (!manifest) {
        application::instance()->logger()->log({ .level = logging::level::error, .message = std::format("failed to parse extension.json in '{}'", dir_path), .tag = "extension/loader" });
        return result;
    }

    if (out_manifest) *out_manifest = *manifest;

    std::string platform_str = "linux-x64";
    std::string dll_name = "lib" + manifest->main + ".so";

    std::string dll_path = platform::join_path(
        platform::join_path(dir_path, "dist"),
        platform::join_path(platform_str, dll_name));

    result = load_extension_from(dll_path);
    if (!result.instance) {
        dll_path = platform::join_path(dir_path, dll_name);
        result = load_extension_from(dll_path);
    }

    return result;
}

} // namespace spiration
