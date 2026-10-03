/**
 * @file thread.cpp
 * @brief 宿主函数表 · 线程（`is_host_thread` / `dispatch`）。
 * @author clk
 *
 * 除 `dispatch` 外，C API 的其余宿主函数都只能在宿主主线程调用。
 */

#include "internal.h"

#include <spiration/application.h>
#include <spiration/utils/dispatcher.h>

#include <memory>
#include <thread>

namespace spiration {
namespace capi {

spiration_bool SPIRATION_CALL host_is_host_thread(
    const spiration_extension_desc* self) {
    SPIRATION_UNUSED(self);
    return std::this_thread::get_id() == host_thread() ? SPIRATION_TRUE
                                                       : SPIRATION_FALSE;
}

spiration_status SPIRATION_CALL host_dispatch(
    const spiration_extension_desc* self, spiration_callback callback,
    void* user_data) {
    if (self == nullptr || callback == nullptr) {
        return fail(SPIRATION_ERR_INVALID_ARGUMENT, "dispatch: null argument");
    }

    application* app = application::instance();
    if (app == nullptr || app->dispatcher() == nullptr) {
        return fail(SPIRATION_ERR_NOT_READY, "dispatch: dispatcher unavailable");
    }

    const std::shared_ptr<std::atomic<bool>> flag = alive_flag(self);
    app->dispatcher()->post_to_ui([flag, callback, user_data]() {
        /* 拓展已关闭则丢弃任务，保证 on_shutdown 之后不再回调。 */
        if (flag && !flag->load()) return;
        callback(user_data);
    });
    return SPIRATION_OK;
}

} // namespace capi
} // namespace spiration
