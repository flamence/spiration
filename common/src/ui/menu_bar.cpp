/**
 * @file menu_bar.cpp
 * @brief 菜单栏控件实现。
 * @author clk
 */

#include <ui/menu_bar.h>
#include <ui/layout.h>
#include <ui/popup_menu.h>
#include <ui/theme.h>
#include <utils/i18n.h>

namespace spiration {

void menu_bar::init() {
    style.background_color = theme::menu_bar_bg();
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

    auto popup = std::make_unique<popup_menu>();
    auto& menu = menus_[index];
    for (auto& sub : menu.sub_items) {
        if (sub.text == "---") {
            popup->add_item("---", nullptr);
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
    item->text = i18n::tr(text, text);
    item->style.width = 60;
    item->height = height;
    item->hover_color = theme::button_hover();
    item->press_color = theme::button_press();
    item->init();

    items_.push_back(item.get());
    add_child(std::move(item));

    menu_desc desc;
    desc.title = text;
    menus_.push_back(std::move(desc));
    return menus_.back();
}

} 
