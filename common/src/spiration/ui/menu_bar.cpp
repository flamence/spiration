/**
 * @file menu_bar.cpp
 * @brief 菜单栏控件实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/ui/menu_bar.h>
#include <spiration/ui/layout.h>
#include <spiration/ui/popup_menu.h>
#include <spiration/ui/theme_manager.h>
#include <spiration/ui/text_field.h>
#include <spiration/extension/builtin/i18n/i18n.h>

namespace spiration {

void menu_bar::init() {
    widget_style.background_color = theme_manager::get(theme_manager::MENU_BAR_BG);
    auto hlayout = std::make_unique<horizontal_layout>(0.0f);
    set_layout_manager(std::move(hlayout));
}

void menu_bar::layout() {
    container::layout();
}

void menu_bar::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        if (md->action == mouse_action::down) {
            for (size_t i = 0; i < items_.size(); ++i) {
                auto* item = items_[i];
                if (md->position.x >= item->x && md->position.x <= item->x + item->width &&
                    md->position.y >= item->y && md->position.y <= item->y + item->height) {
                    md->consumed = true;
                    active_index_ = static_cast<int>(i);
                    show_menu_popup(static_cast<int>(i));
                    return;
                }
            }
            active_index_ = -1;
        }
    }
    container::handle_event(type, data);
}

void menu_bar::show_menu_popup(int index) {
    if (index < 0 || index >= static_cast<int>(menus_.size()) || !show_popup_) return;
    text_field::blur_current();

    auto popup = std::make_unique<popup_menu>();
    auto& menu = menus_[index];
    for (auto& sub : menu.sub_items) {
        if (sub.separator) {
            popup->add_separator();
        } else {
            popup->add_item(sub.text, sub.callback);
        }
    }

    
    auto* item = items_[index];
    float absX = x + item->x;
    float absY = y + item->y + height;
    for (auto* p = parent(); p; p = p->parent()) {
        absX += p->x;
        absY += p->y;
    }
    show_popup_(absX, absY, std::move(popup));
}

menu_desc& menu_bar::add_menu(const std::string& text) {
    auto item = std::make_unique<menu_item>();
    item->text = i18n_manager::get().tr(text, text);
    item->widget_style.width = 60;
    item->height = height;
    item->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    item->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    item->init();

    items_.push_back(item.get());
    add_child(std::move(item));

    menu_desc desc;
    desc.title = text;
    menus_.push_back(std::move(desc));
    return menus_.back();
}

bool menu_bar::add_sub_item(const std::string& menu_title,
                             const std::string& label,
                             std::function<void()> callback,
                             const std::string& owner) {
    for (auto& menu : menus_) {
        if (menu.title == menu_title
            || i18n_manager::get().tr(menu.title) == menu_title) {
            if (menu.tail_index != menu_desc::NO_TAIL) {
                menu.sub_items.insert(
                    menu.sub_items.begin() + static_cast<std::ptrdiff_t>(menu.tail_index),
                    menu_desc::item{label, std::move(callback), false, owner});
                ++menu.tail_index;
            } else {
                menu.sub_items.push_back({label, std::move(callback), false, owner});
            }
            return true;
        }
    }
    return false;
}

size_t menu_bar::remove_owned_items(const std::string& owner) {
    if (owner.empty()) return 0;

    size_t removed = 0;
    for (size_t index = 0; index < menus_.size();) {
        menu_desc& menu = menus_[index];
        const size_t before = menu.sub_items.size();
        menu.sub_items.erase(
            std::remove_if(menu.sub_items.begin(), menu.sub_items.end(),
                           [&owner](const menu_desc::item& entry) {
                               return entry.owner == owner;
                           }),
            menu.sub_items.end());
        removed += before - menu.sub_items.size();

        if (menu.tail_index != menu_desc::NO_TAIL &&
            menu.tail_index > menu.sub_items.size()) {
            menu.tail_index = menu.sub_items.size();
        }

        if (!menu.sub_items.empty()) {
            ++index;
            continue;
        }

        /* 该菜单已被清空：连同其控件一起移除。 */
        if (index < items_.size() && items_[index] != nullptr) {
            std::unique_ptr<widget> detached = remove_child(items_[index]);
            detached.reset();
            items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
        }
        menus_.erase(menus_.begin() + static_cast<std::ptrdiff_t>(index));
        invalidate_layout();
    }
    return removed;
}

} // namespace spiration
