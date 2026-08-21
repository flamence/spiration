/**
 * @file extension_manager.mm
 * @brief 拓展管理器实现。
 * @author clk
 */

#include <dlfcn.h>
#include <fmt/format.h>
#include <spiration/application.h>
#include <spiration/extension/extension_manager.h>
#include <string>

#import <Foundation/Foundation.h>

namespace spiration {

void library_deleter::operator()(library_handle* handle) const {
    if (!handle) return;
    if (handle->mod) dlclose(handle->mod);
    delete handle;
}

void* extension_manager::find_symbol(library_handle* handle, const std::string& name) {
    if (!handle || !handle->mod) {
        application::instance()->logger()->log({
            logging::level::error, "invalid library handle", "extension/loader"});
        return nullptr;
    }

    void* ptr = dlsym(handle->mod, name.c_str());
    if (!ptr) {
        application::instance()->logger()->log({
            logging::level::error,
            fmt::format("dlsym: {} failed: {}", name, dlerror()),
            "extension/loader"});
    }

    return ptr;
}

library_ptr extension_manager::load_library(const ghc::filesystem::path& path) {
    auto* handle = new library_handle();
    library_ptr result(handle);

    ghc::filesystem::path preferred = path;
    preferred.make_preferred();

    handle->mod = dlopen(preferred.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle->mod) {
        application::instance()->logger()->log({
            logging::level::warning,
            fmt::format("dlopen failed: {} when loading {}", dlerror(), preferred.string()),
            "extension/manager"});
        return library_ptr(nullptr);
    }

    application::instance()->logger()->log({
        logging::level::trace,
        fmt::format("loaded library successfully: {}", preferred.string()),
        "extension/manager"});
    return result;
}

} // namespace spiration
