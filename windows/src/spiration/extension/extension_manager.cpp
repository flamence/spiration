/**
 * @file extension_manager.cpp
 * @brief 拓展管理器实现。
 * @author clk
 */

#include <spiration/extension/extension_manager.h>

namespace spiration {

void library_deleter::operator()(library_handle* handle) const {
    if (!handle) return;
    if (handle->mod) FreeLibrary(static_cast<HMODULE>(handle->mod));
    delete handle;
}

void* extension_manager::find_symbol(library_handle* handle, const std::string& name) {
    if (!handle || !handle->mod) {
        application::instance()->logger()->log({logging::level::error, "invalid library handle", "extension/loader"});
        return nullptr;
    }

    void* ptr = reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle->mod), name.c_str()));
    if (!ptr) {
        application::instance()->logger()->log({logging::level::error, fmt::format("GetProcAddress: {} failed", name), "extension/loader"});
    }

    return ptr;
}

library_ptr extension_manager::load_library(const ghc::filesystem::path& path) {
    auto* handle = new library_handle();
    library_ptr result(handle);
    ghc::filesystem::path preferred = path;
    preferred.make_preferred();
    auto cokkie = AddDllDirectory(preferred.parent_path().wstring().c_str());
    handle->mod = reinterpret_cast<void*>(LoadLibraryExW(preferred.wstring().c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH));
    if (!handle->mod) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("LoadLibraryW failed with error {} when loading {}", std::to_string(GetLastError()), preferred.string()), "extension/manager"});
        return library_ptr(nullptr);
    }
    if (cokkie) {
        RemoveDllDirectory(cokkie);
    }
    application::instance()->logger()->log({logging::level::trace, fmt::format("loaded library successfully: {}", preferred.string()), "extension/manager"});
    return result;
}

}
