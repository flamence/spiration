/**
 * @file tab.cpp
 * @brief `spiration.ui` · 标签页与菜单项（含卸载清理）。
 * @author clk
 *
 * 拓展创建标签页与菜单项时一并登记归属；拓展卸载时由 `release_ui_owned`
 * 逐项回收——否则用户在拓展卸载后点击遗留的按钮或菜单项，宿主就会跳转进
 * 已经 `dlclose` 的动态库。
 */

#include "internal.h"
#include "internal_ui.h"

#include <spiration/application.h>
#include <spiration/ui/root.h>

#include <memory>
#include <string>
#include <vector>

namespace spiration {
namespace capi {

spiration_status SPIRATION_CALL ui_add_menu_item(
    const spiration_extension_desc* self, spiration_str_view menu,
    spiration_str_view label, spiration_callback callback, void* user_data) {
    if (callback == nullptr) {
        return SPIRATION_ERR_INVALID_ARGUMENT;
    }

    root* root_widget = dynamic_cast<root*>(application::instance()->widget());
    if (root_widget == nullptr) {
        return SPIRATION_ERR_NOT_READY;
    }

    const std::string owner = text_of(self != nullptr ? self->id : spiration_str_view());
    const bool ok = root_widget->add_menu_item(
        text_of(menu), text_of(label),
        [callback, user_data]() { callback(user_data); }, owner);
    return ok ? SPIRATION_OK : SPIRATION_ERR_NOT_FOUND;
}

spiration_tab* SPIRATION_CALL ui_tab_open(
    const spiration_extension_desc* self, spiration_str_view title) {
    root* root_widget = dynamic_cast<root*>(application::instance()->widget());
    if (root_widget == nullptr) return NULL;

    std::unique_ptr<tab_handle> created(new tab_handle());
    created->set_title(text_of(title));

    tab_handle* raw = created.get();
    root_widget->open_tab(std::move(created));

    /* 登记归属，供拓展卸载时关闭。 */
    owned_tab entry;
    entry.owner = self;
    entry.handle = raw;
    owned_tabs().push_back(entry);

    return reinterpret_cast<spiration_tab*>(raw);
}

void SPIRATION_CALL ui_tab_close(spiration_tab* handle) {
    tab_handle* target = as_handle(handle);
    if (target == nullptr) return;

    root* root_widget = nullptr;
    tab_bar* strip = tab_strip(root_widget);
    if (strip == nullptr) return;

    for (int i = 0; i < strip->tab_count(); ++i) {
        if (strip->get_tab(i) == target) {
            strip->close_tab(i);
            return;
        }
    }
}

spiration_status SPIRATION_CALL ui_tab_activate(spiration_tab* handle) {
    tab_handle* target = as_handle(handle);
    if (target == nullptr) return SPIRATION_ERR_NOT_FOUND;

    root* root_widget = nullptr;
    tab_bar* strip = tab_strip(root_widget);
    if (strip == nullptr) return SPIRATION_ERR_NOT_READY;

    for (int i = 0; i < strip->tab_count(); ++i) {
        if (strip->get_tab(i) == target) {
            strip->activate_tab(i);
            return SPIRATION_OK;
        }
    }
    return SPIRATION_ERR_NOT_FOUND;
}

spiration_status SPIRATION_CALL ui_tab_set_title(spiration_tab* handle,
                                                 spiration_str_view title) {
    tab_handle* target = as_handle(handle);
    if (target == nullptr) return SPIRATION_ERR_NOT_FOUND;
    target->set_title(text_of(title));
    repaint();
    return SPIRATION_OK;
}

spiration_status SPIRATION_CALL ui_tab_set_content(spiration_tab* handle,
                                                   spiration_widget* content) {
    tab_handle* target = as_handle(handle);
    if (target == nullptr) return SPIRATION_ERR_NOT_FOUND;

    if (content == nullptr) {
        target->set_content(nullptr);
        repaint();
        return SPIRATION_OK;
    }

    widget_handle* box = as_handle(content);
    if (box == nullptr) return SPIRATION_ERR_NOT_FOUND;
    if (box->parent() != nullptr) {
        return SPIRATION_ERR_INVALID_ARGUMENT; /* 已挂在某棵树上 */
    }

    target->set_content(std::unique_ptr<widget>(box));
    repaint();
    return SPIRATION_OK;
}

spiration_bool SPIRATION_CALL ui_tab_is_alive(const spiration_tab* handle) {
    if (handle == nullptr) return SPIRATION_FALSE;
    return live_tabs().count(handle) != 0u ? SPIRATION_TRUE : SPIRATION_FALSE;
}

void release_ui_owned(const spiration_extension_desc* owner) {
    if (owner == nullptr) return;

    /* 1. 关闭该拓展打开的标签页：控件树随之一并销毁，其中的回调自然失效。 */
    std::vector<owned_tab> closing;
    std::vector<owned_tab> keeping;
    for (size_t i = 0; i < owned_tabs().size(); ++i) {
        if (owned_tabs()[i].owner == owner) {
            closing.push_back(owned_tabs()[i]);
        } else {
            keeping.push_back(owned_tabs()[i]);
        }
    }
    owned_tabs().swap(keeping);

    for (size_t i = 0; i < closing.size(); ++i) {
        tab_handle* target =
            as_handle(reinterpret_cast<spiration_tab*>(closing[i].handle));
        if (target == nullptr) continue;

        root* root_widget = nullptr;
        tab_bar* strip = tab_strip(root_widget);
        if (strip == nullptr) continue;
        for (int index = 0; index < strip->tab_count(); ++index) {
            if (strip->get_tab(index) == target) {
                strip->close_tab(index);
                break;
            }
        }
    }

    /* 2. 移除该拓展添加的菜单项。 */
    root* root_widget = dynamic_cast<root*>(application::instance()->widget());
    if (root_widget != nullptr) {
        root_widget->remove_menu_items(text_of(owner->id));
    }
    repaint();
}

} // namespace capi
} // namespace spiration
