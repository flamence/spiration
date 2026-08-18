/**
 * @file extension_loader.cpp
 * @brief 拓展加载器实现。
 * @author clk
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <libloaderapi.h>

#include <spiration/application.h>
#include <spiration/extension/extension_loader.h>
#include <spiration/extension/extension.h>
#include <ghc/filesystem.hpp>
#include <spiration/utils/io.h>

#include <fmt/format.h>
#include <string>

namespace spiration {

void extension_loader::library_deleter::operator()(
    library_handle* handle) const {
    if (!handle) return;
    if (handle->mod) FreeLibrary(static_cast<HMODULE>(handle->mod));
    delete handle;
}

extension_loader::lib_handle extension_loader::load_library(const ghc::filesystem::path& path) {
    auto* handle = new library_handle();
    lib_handle result(handle);

    ghc::filesystem::path preferred = path;
    preferred.make_preferred();
    const std::wstring dll_path_w = preferred.wstring();

    auto cokkie = AddDllDirectory(preferred.parent_path().wstring().c_str());

    handle->mod = reinterpret_cast<void*>(LoadLibraryExW(dll_path_w.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH));
    if (!handle->mod) {
        application::instance()->logger()->log({logging::level::error, fmt::format("LoadLibraryW failed with error {}", std::to_string(GetLastError())), "extension/loader"});
        return lib_handle(nullptr);
    }

    if (cokkie) {
        RemoveDllDirectory(cokkie);
    }

    application::instance()->logger()->log({logging::level::info, fmt::format("loaded library \"{}\"", path.string()), "extension/loader"});
    return result;
}

void* extension_loader::find_symbol(library_handle* handle, const std::string& symbol_name) {
    if (!handle || !handle->mod) {
        application::instance()->logger()->log({logging::level::error, "invalid library handle", "extension/loader"});
        return nullptr;
    }

    void* ptr = reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle->mod), symbol_name.c_str()));
    if (!ptr) {
        application::instance()->logger()->log({logging::level::error, fmt::format("GetProcAddress: {} failed", symbol_name), "extension/loader"});
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

    try {
        application::instance()->logger()->log({logging::level::info, fmt::format("loaded extension \"{}\" v{} from \"{}\"", ext->name(), ext->version(), path.string()), "extension/loader"});
    } catch (...) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("the library \"{}\" is broken", path.string()), "extension/loader"});
        return result;
    }
    
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
