/**
 * @file ui.cpp
 * @brief `spiration.ui` 可选接口 · 共享设施与函数表组装。
 * @author clk
 *
 * 本文件提供三个界面翻译单元共享的部分：
 *
 * - 存活句柄集合与标签页归属登记（单例）；
 * - 重绘请求与标签栏取用；
 * - `spiration_ui_api` 的组装与版本协商。
 *
 * 具体实现见 `widget.cpp`（控件）与 `tab.cpp`（标签页、菜单）。
 */

#include "internal.h"
#include "internal_ui.h"

#include <spiration/application.h>
#include <spiration/ui/root.h>
#include <spiration/window/window.h>

#include <cstring>
#include <set>
#include <vector>

namespace spiration {
namespace capi {

/* -------------------------------------------------------------------------
 * 共享单例
 * ---------------------------------------------------------------------- */

std::set<const void*>& live_widgets() {
    static std::set<const void*> handles;
    return handles;
}

std::set<const void*>& live_tabs() {
    static std::set<const void*> handles;
    return handles;
}

std::vector<owned_tab>& owned_tabs() {
    static std::vector<owned_tab> entries;
    return entries;
}

/* -------------------------------------------------------------------------
 * 公共操作
 * ---------------------------------------------------------------------- */

void repaint() {
    application* app = application::instance();
    if (app != nullptr && app->window() != nullptr) {
        app->window()->request_repaint();
    }
}

tab_bar* tab_strip(root*& out_root) {
    out_root = dynamic_cast<root*>(application::instance()->widget());
    if (out_root == nullptr) return nullptr;
    return out_root->get_tab_bar();
}

/* -------------------------------------------------------------------------
 * 函数表
 * ---------------------------------------------------------------------- */

namespace {

void SPIRATION_CALL ui_request_repaint(const spiration_extension_desc* self) {
    SPIRATION_UNUSED(self);
    repaint();
}

/** @brief 组装界面函数表。 */
spiration_ui_api make_ui_api() {
    spiration_ui_api api;
    std::memset(&api, 0, sizeof(api));

    api.version = SPIRATION_UI_VERSION;

    api.request_repaint = ui_request_repaint;
    api.add_menu_item = ui_add_menu_item;

    api.tab_open = ui_tab_open;
    api.tab_close = ui_tab_close;
    api.tab_activate = ui_tab_activate;
    api.tab_set_title = ui_tab_set_title;
    api.tab_set_content = ui_tab_set_content;
    api.tab_is_alive = ui_tab_is_alive;

    api.widget_create = ui_widget_create;
    api.widget_destroy = ui_widget_destroy;
    api.widget_append = ui_widget_append;
    api.widget_set = ui_widget_set;
    api.widget_get = ui_widget_get;
    api.widget_listen = ui_widget_listen;

    api.size = static_cast<uint32_t>(sizeof(api));
    return api;
}

} // namespace

const spiration_ui_api* ui_interface(uint32_t required_version) {
    /* 主版本必须一致，且宿主实现不得低于请求的次版本。 */
    if (SPIRATION_API_VERSION_MAJOR_OF(required_version) !=
        SPIRATION_API_VERSION_MAJOR_OF(SPIRATION_UI_VERSION)) {
        return NULL;
    }
    if (required_version > SPIRATION_UI_VERSION) return NULL;

    static const spiration_ui_api api = make_ui_api();
    return &api;
}

} // namespace capi
} // namespace spiration
