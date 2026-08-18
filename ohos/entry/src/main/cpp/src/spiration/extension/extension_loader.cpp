/**
 * @file extension_loader.cpp
 * @brief 拓展加载器实现。
 * @author clk
 */

#include <dlfcn.h>
#include <spiration/application.h>
#include <spiration/extension/extension_loader.h>
#include <spiration/extension/extension.h>
#include <spiration/utils/io.h>
#include <string>

namespace spiration {

void extension_loader::library_deleter::operator()(
    library_handle* handle) const {
    if (!handle) return;
    if (handle->mod) dlclose(handle->mod);
    delete handle;
}

extension_loader::lib_handle extension_loader::load_library(const ghc::filesystem::path& path) {
    auto* handle = new library_handle();
    lib_handle result(handle);

    handle->mod = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle->mod) {
        application::instance()->logger()->log({logging::level::error, fmt::format("{}", dlerror()), "extension/loader"});
        return lib_handle(nullptr);
    }

    application::instance()->logger()->log({logging::level::info, fmt::format("loaded library \"{}\"", path.string()), "extension/loader"});
    return result;
}

void* extension_loader::find_symbol(library_handle* handle, const std::string& symbol_name) {
    if (!handle || !handle->mod) {
        application::instance()->logger()->log({logging::level::error, "invalid library handle", "extension/loader"});
        return nullptr;
    }

    void* ptr = dlsym(handle->mod, symbol_name.c_str());
    if (!ptr) {
        application::instance()->logger()->log({logging::level::error, fmt::format("{}", dlerror()), "extension/loader"});
    }

    return ptr;
}

extension_loader::load_result extension_loader::load_extension_from(const ghc::filesystem::path& path) {
    load_result result;

    auto handle = load_library(path);
    if (!handle) return result;

    auto create_fn = reinterpret_cast<extension_create_func>(
        find_symbol(handle.get(), "create_extension"));
    if (!create_fn) {
        application::instance()->logger()->log({logging::level::error, fmt::format("no \"create_extension\" symbol in \"{}\"", path.string()), "extension/loader"});
        return result;
    }

    extension* ext = create_fn();
    if (!ext) {
        application::instance()->logger()->log({logging::level::error, fmt::format("create_extension() returned null from \"{}\"", path.string()), "extension/loader"});
        return result;
    }

    result.handle = std::move(handle);
    result.instance = ext;

    application::instance()->logger()->log({logging::level::info, fmt::format("loaded extension \"{}\" v{} from \"{}\"", ext->name(), ext->version(), path.string()), "extension/loader"});
    return result;
}

extension_loader::load_result extension_loader::load_extension_from_dir(
    const ghc::filesystem::path& dir_path, manifest_data* out_manifest) {
    load_result result;

    ghc::filesystem::path manifest_path = dir_path / "extension.json";
    std::string manifest_json = io::read_file_content(manifest_path);

    if (manifest_json.empty()) {
        application::instance()->logger()->log({logging::level::error, fmt::format("no extension.json found in '{}'", dir_path.string()), "extension/loader"});
        return result;
    }

    auto manifest = parse_extension_manifest(manifest_json);
    if (!manifest) {
        application::instance()->logger()->log({logging::level::error, fmt::format("failed to parse extension.json in '{}'", dir_path.string()), "extension/loader"});
        return result;
    }

    if (out_manifest) *out_manifest = *manifest;

    ghc::filesystem::path dll_path = dir_path / manifest->main;

    result = load_extension_from(dll_path);

    return result;
}

} // namespace spiration
