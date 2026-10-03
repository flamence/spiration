/**
 * @file registry.cpp
 * @brief 宿主函数表 · 拓展注册表（`find_extension` / `enumerate_extensions`）。
 * @author clk
 *
 * 以 C API 装入的拓展返回其**真实描述符**；以原生 C++ API 实现的拓展则由
 * `shim()` 构造一个只有身份字段有效的**垫片描述符**。
 */

#include "internal.h"

#include <spiration/extension/extension.hpp>
#include <spiration/extension/extension_manager.h>

#include <string>
#include <vector>

namespace spiration {
namespace capi {

spiration_extension_desc* SPIRATION_CALL host_find_extension(
    const spiration_extension_desc* self, spiration_str_view id) {
    SPIRATION_UNUSED(self);
    const std::string target = text_of(id);

    auto it = global_state().descriptors.find(target);
    if (it != global_state().descriptors.end()) return it->second;

    extension_manager* mgr = manager();
    if (mgr == nullptr) return NULL;
    extension* found = mgr->find_extension(target);
    if (found == nullptr) return NULL;
    return const_cast<spiration_extension_desc*>(shim(found));
}

size_t SPIRATION_CALL host_enumerate_extensions(
    const spiration_extension_desc* self,
    const spiration_extension_desc** out, size_t capacity) {
    SPIRATION_UNUSED(self);
    extension_manager* mgr = manager();
    if (mgr == nullptr) return 0u;

    std::vector<const spiration_extension_desc*> all;
    for (extension* ext : mgr->extensions()) {
        if (ext == nullptr) continue;

        const std::string id = ext->id();
        auto it = global_state().descriptors.find(id);
        if (it != global_state().descriptors.end()) {
            all.push_back(it->second);
        } else {
            all.push_back(shim(ext));
        }
    }

    if (out != NULL) {
        const size_t count = all.size() < capacity ? all.size() : capacity;
        for (size_t i = 0; i < count; ++i) out[i] = all[i];
    }
    return all.size();
}

} // namespace capi
} // namespace spiration
