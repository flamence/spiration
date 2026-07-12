/**
 * @file extension_loader.cpp
 * @brief 跨平台动态库加载器实现。
 * @author clk
 */

#include <extension/extension_loader.h>
#include <extension/extension.h>
#include <extension/extension_api.h>
#include <utils/console.h>

#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace spiration {

std::string extension_loader::s_last_error;

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
        DWORD err = GetLastError();
        s_last_error = "LoadLibraryW failed with error " + std::to_string(err);
        console::error("extension_loader: %s", s_last_error.c_str());
        return lib_handle(nullptr);
    }
#else
    handle->mod = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle->mod) {
        s_last_error = dlerror();
        console::error("extension_loader: %s", s_last_error.c_str());
        return lib_handle(nullptr);
    }
#endif

    console::info("extension_loader: loaded library '%s'", path.c_str());
    return result;
}

void* extension_loader::find_symbol(library_handle* handle, const std::string& symbol_name) {
    if (!handle || !handle->mod) {
        s_last_error = "invalid library handle";
        return nullptr;
    }

#ifdef _WIN32
    void* ptr = reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle->mod), symbol_name.c_str()));
    if (!ptr) {
        s_last_error = "GetProcAddress: " + symbol_name + " failed";
    }
#else
    void* ptr = dlsym(handle->mod, symbol_name.c_str());
    if (!ptr) {
        s_last_error = dlerror();
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
        console::error("extension_loader: no 'create_extension' symbol in '%s'",
                       path.c_str());
        return result;
    }

    extension* ext = create_fn();
    if (!ext) {
        console::error("extension_loader: create_extension() returned null from '%s'",
                       path.c_str());
        return result;
    }

    result.handle = std::move(handle);
    result.instance = ext;

    console::info("extension_loader: loaded extension '%s' v%s from '%s'",
                  ext->name().c_str(), ext->version().c_str(), path.c_str());
    return result;
}

std::string extension_loader::last_error() {
    return s_last_error;
}

} // namespace spiration
