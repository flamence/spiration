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

#include <extension/extension_loader.h>
#include <extension/extension.h>
#include <filesystem>
#include <utils/console.h>
#include <utils/io.h>

#include <string>

namespace spiration {

void extension_loader::library_deleter::operator()(
    library_handle* handle) const {
    if (!handle) return;
    if (handle->mod) FreeLibrary(static_cast<HMODULE>(handle->mod));
    delete handle;
}

extension_loader::lib_handle extension_loader::load_library(const std::filesystem::path& path) {
    auto* handle = new library_handle();
    lib_handle result(handle);

    auto cokkie = AddDllDirectory(path.parent_path().wstring().c_str());

    handle->mod = reinterpret_cast<void*>(LoadLibraryExW(path.wstring().c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH));
    if (!handle->mod) {
        console::error("extension/loader", "LoadLibraryW failed with error %s",
                       std::to_string(GetLastError()).c_str());
        return lib_handle(nullptr);
    }

    if (cokkie) {
        RemoveDllDirectory(cokkie);
    }

    console::info("extension/loader", "loaded library \"%s\"", path.c_str());
    return result;
}

void* extension_loader::find_symbol(library_handle* handle, const std::string& symbol_name) {
    if (!handle || !handle->mod) {
        console::error("extension/loader", "invalid library handle");
        return nullptr;
    }

    void* ptr = reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle->mod), symbol_name.c_str()));
    if (!ptr) {
        console::error("extension/loader", "GetProcAddress: %s failed", symbol_name.c_str());
    }

    return ptr;
}

extension_loader::load_result extension_loader::load_extension_from(const std::filesystem::path& path) {
    load_result result;

    auto handle = load_library(path);
    if (!handle) return result;

    auto create_fn = reinterpret_cast<extension_create_func>(
        find_symbol(handle.get(), "create_extension"));
    if (!create_fn) {
        console::error("extension/loader", "no \"create_extension\" symbol in \"%s\"",
                       path.string().c_str());
        return result;
    }

    extension* ext = create_fn();
    if (!ext) {
        console::error("extension/loader", "create_extension() returned null from \"%s\"",
                       path.string().c_str());
        return result;
    }

    result.handle = std::move(handle);
    result.instance = ext;

    console::info("extension/loader", "loaded extension \"%s\" v%s from \"%s\"",
                  ext->name().c_str(), ext->version().c_str(), path.c_str());
    return result;
}

extension_loader::load_result extension_loader::load_extension_from_dir(
    const std::filesystem::path& dir_path, manifest_data* out_manifest) {
    load_result result;

    std::filesystem::path manifest_path = dir_path / "extension.json";
    std::string manifest_json = io::read_file_content(manifest_path);

    if (manifest_json.empty()) {
        console::error("extension/loader", "no extension.json or .dll found in '%s'",
                       dir_path.c_str());
        return result;
    }

    auto manifest = parse_extension_manifest(manifest_json);
    if (!manifest) {
        console::error("extension/loader", "failed to parse extension.json in '%s'",
                       dir_path.c_str());
        return result;
    }

    if (out_manifest) *out_manifest = *manifest;

    std::filesystem::path dll_path = dir_path / manifest->main;

    result = load_extension_from(dll_path);

    return result;
}

} // namespace spiration
