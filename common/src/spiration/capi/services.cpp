/**
 * @file services.cpp
 * @brief 宿主函数表 · 拓展间服务（`register_service` / `query_service`）。
 * @author clk
 *
 * 服务是裸指针，语义完全由提供方约定，宿主不做任何类型检查。
 */

#include "internal.h"

#include <spiration/extension/extension_manager.h>

#include <string>

namespace spiration {
namespace capi {

spiration_status SPIRATION_CALL host_register_service(
    const spiration_extension_desc* self, spiration_str_view name,
    void* service) {
    if (self == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT,
                    "register_service: null descriptor");
    }
    extension_manager* mgr = manager();
    if (mgr == nullptr) {
        return fail(SPIRATION_ERR_NOT_READY, "register_service: unavailable");
    }
    mgr->register_service(text_of(self->id), text_of(name), service);
    return SPIRATION_OK;
}

void* SPIRATION_CALL host_query_service(const spiration_extension_desc* self,
                                        spiration_str_view extension_id,
                                        spiration_str_view name) {
    SPIRATION_UNUSED(self);
    extension_manager* mgr = manager();
    if (mgr == nullptr) return NULL;
    return mgr->get_service(text_of(extension_id), text_of(name));
}

} // namespace capi
} // namespace spiration
