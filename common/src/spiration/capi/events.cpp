/**
 * @file events.cpp
 * @brief 宿主函数表 · 事件（`subscribe` / `unsubscribe` / `publish`）。
 * @author clk
 *
 * 订阅者即拓展本身：宿主按描述符记录订阅 ID，拓展卸载时统一退订。
 */

#include "internal.h"

#include <spiration/extension/extension_manager.h>

#include <string>
#include <vector>

namespace spiration {
namespace capi {

int64_t SPIRATION_CALL host_subscribe(const spiration_extension_desc* self,
                                      spiration_str_view event,
                                      spiration_event_callback callback,
                                      void* user_data) {
    extension_manager* mgr = manager();
    if (mgr == nullptr || self == nullptr || callback == nullptr) return 0;

    const std::string event_name = text_of(event);
    auto forward = [callback, user_data, event_name](const std::string& data) {
        callback(user_data, view_of(event_name),
                 spiration_str_view_of(data.data(), data.size()));
    };

    const int subscription = mgr->on_event(event_name, forward);
    if (subscription <= 0) return 0;

    global_state().subscriptions[self].push_back(subscription);
    return static_cast<int64_t>(subscription);
}

void SPIRATION_CALL host_unsubscribe(const spiration_extension_desc* self,
                                     int64_t subscription) {
    extension_manager* mgr = manager();
    if (mgr == nullptr || subscription <= 0) return;

    mgr->off_event(static_cast<int>(subscription));

    auto it = global_state().subscriptions.find(self);
    if (it == global_state().subscriptions.end()) return;
    std::vector<int64_t>& owned = it->second;
    for (size_t i = 0; i < owned.size(); ++i) {
        if (owned[i] == subscription) {
            owned.erase(owned.begin() + static_cast<long>(i));
            break;
        }
    }
}

spiration_status SPIRATION_CALL host_publish(
    const spiration_extension_desc* self, spiration_str_view event,
    spiration_str_view data) {
    SPIRATION_UNUSED(self);
    extension_manager* mgr = manager();
    if (mgr == nullptr) {
        return fail(SPIRATION_ERR_NOT_READY, "publish: manager unavailable");
    }
    mgr->emit_event(text_of(event), text_of(data));
    return SPIRATION_OK;
}

} // namespace capi
} // namespace spiration
