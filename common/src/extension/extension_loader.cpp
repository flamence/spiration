/**
 * @file extension_loader.cpp
 * @brief 拓展加载器实现。
 * @author clk
 */

#include <extension/extension_loader.h>
#include <extension/extension.h>

#include <fstream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace spiration {

void extension_loader::library_deleter::operator()(
    library_handle* handle) const {
    if (!handle) return;
#ifdef _WIN32
    if (handle->mod) FreeLibrary(static_cast<HMODULE>(handle->mod));
#else
    if (handle->mod) dlclose(handle->mod);
#endif
    delete handle;
}

extension_loader::lib_handle extension_loader::load_library(const std::string& path) {
    auto* handle = new library_handle();
    lib_handle result(handle);

#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

    handle->mod = reinterpret_cast<void*>(LoadLibraryW(wpath.c_str()));
    if (!handle->mod) {
        console::error("extension/loader", "LoadLibraryW failed with error %s",
                       std::to_string(GetLastError()).c_str());
        return lib_handle(nullptr);
    }
#else
    handle->mod = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle->mod) {
        console::error("extension/loader", "%s", dlerror());
        return lib_handle(nullptr);
    }
#endif

    console::info("extension/loader", "loaded library \"%s\"", path.c_str());
    return result;
}

void* extension_loader::find_symbol(library_handle* handle, const std::string& symbol_name) {
    if (!handle || !handle->mod) {
        console::error("extension/loader", "invalid library handle");
        return nullptr;
    }

#ifdef _WIN32
    void* ptr = reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle->mod), symbol_name.c_str()));
    if (!ptr) {
        console::error("extension/loader", "GetProcAddress: %s failed", symbol_name.c_str());
    }
#else
    void* ptr = dlsym(handle->mod, symbol_name.c_str());
    if (!ptr) {
        console::error("extension/loader", "%s", dlerror());
    }
#endif

    return ptr;
}

extension_loader::load_result extension_loader::load_extension_from(const std::string& path) {
    load_result result;

    auto handle = load_library(path);
    if (!handle) return result;

    auto create_fn = reinterpret_cast<extension_create_func>(
        find_symbol(handle.get(), "create_extension"));
    if (!create_fn) {
        console::error("extension/loader", "no \"create_extension\" symbol in \"%s\"",
                       path.c_str());
        return result;
    }

    extension* ext = create_fn();
    if (!ext) {
        console::error("extension/loader", "create_extension() returned null from \"%s\"",
                       path.c_str());
        return result;
    }

    result.handle = std::move(handle);
    result.instance = ext;

    console::info("extension/loader", "loaded extension \"%s\" v%s from \"%s\"",
                  ext->name().c_str(), ext->version().c_str(), path.c_str());
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
#ifdef _WIN32
            if (entry.size() >= 4 && entry.substr(entry.size() - 4) == ".dll")
#else
            if (entry.size() >= 3 && entry.substr(entry.size() - 3) == ".so")
#endif
            {
                std::string dll_path = platform::join_path(dir_path, entry);
                result = load_extension_from(dll_path);
                if (result.instance) return result;
            }
        }
        console::error("extension/loader", "no extension.json or .dll/.so found in '%s'",
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

    std::string platform_str;
#ifdef _WIN32
    platform_str = "win-x64";
    std::string dll_name = manifest->main + ".dll";
#elif defined(__APPLE__)
#ifdef __aarch64__
    platform_str = "macos-arm64";
#else
    platform_str = "macos-x64";
#endif
    std::string dll_name = "lib" + manifest->main + ".dylib";
#else
    platform_str = "linux-x64";
    std::string dll_name = "lib" + manifest->main + ".so";
#endif

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
