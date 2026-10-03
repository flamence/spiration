/**
 * @file loader.cpp
 * @brief 宿主函数表 · 加载器（`register_loader` / `query_loader`）与注册表。
 * @author clk
 *
 * 加载器让脚本语言（Python / Node.js …）编写拓展：它们自行启动解释器，
 * 并把脚本里的函数映射为 `spiration_extension_desc`。
 */

#include "internal.h"

#include <string>

namespace spiration {
namespace capi {

spiration_status register_loader(const std::string& loader_id,
                                 const spiration_loader_api* loader) {
    if (loader_id.empty() || loader == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_loader: null argument");
    }
    if (loader->instantiate == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_loader: loader has no instantiate");
    }
    if (!SPIRATION_API_VERSION_COMPATIBLE(host_api()->version,
                                          loader->version)) {
        return fail(SPIRATION_ERR_UNSUPPORTED,
                    "register_loader: incompatible loader version");
    }

    global_state().loaders[loader_id] = loader;

    if (std::shared_ptr<logging::logger> out = logger()) {
        out->log({logging::level::info,
                  "loader \"" + loader_id + "\" registered", "capi"});
    }
    return SPIRATION_OK;
}

const spiration_loader_api* find_loader(const std::string& loader_id) {
    auto it = global_state().loaders.find(loader_id);
    return it == global_state().loaders.end() ? nullptr : it->second;
}

spiration_status SPIRATION_CALL host_register_loader(
    const spiration_extension_desc* self, const spiration_loader_api* loader) {
    if (self == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_loader: null descriptor");
    }
    return register_loader(text_of(self->id), loader);
}

const spiration_loader_api* SPIRATION_CALL host_query_loader(
    const spiration_extension_desc* self, spiration_str_view loader_id) {
    SPIRATION_UNUSED(self);
    return find_loader(text_of(loader_id));
}

} // namespace capi
} // namespace spiration
