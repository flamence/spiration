/**
 * @file menu_bar.h
 * @brief 菜单栏控件，包含多个菜单项。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/button.h>
#include <ui/popup_menu.h>
#include <ui/theme.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

/**
 * @brief 菜单项。
 */
class menu_item : public button {
public:
    void paint(std::shared_ptr<renderer> renderer) override {
        
        renderer->draw_rectangle({x, y, width, height}, bg_transition_.current());
        renderer->draw_text_aligned(text, rectangle{ x, y, width, height }, theme::get(theme::MENU_TEXT),
                                    text_alignment::center, vertical_alignment::center, 14.0f);
    }
};

/**
 * @brief 菜单条目结构，包含标题和子菜单项列表。
 */
struct menu_desc {
    std::string title;
    struct item {
        std::string text;
        std::function<void()> callback;
        bool separator = false;
    };
    std::vector<item> sub_items;

    void add_separator() { sub_items.push_back({"", nullptr, true}); }
};

/**
 * @brief 顶部菜单栏，包含一排菜单项及弹出子菜单。
 */
class menu_bar : public container {
public:
    void init() override;
    void layout() override;
    void handle_event(const event_type& type, void* data) override;

    /**
     * @brief 添加一个顶级菜单。
     * @param text 菜单标题
     * @return menu_desc& 引用，用于添加子项
     */
    menu_desc& add_menu(const std::string& text);

    /**
     * @brief 在已存在的菜单中添加子项。
     * @param menu_title 目标菜单标题
     * @param label 子项标签
     * @param callback 点击回调
     * @return true 添加成功，false 未找到目标菜单
     */
    bool add_sub_item(const std::string& menu_title,
                      const std::string& label,
                      std::function<void()> callback);

    void set_show_popup_callback(std::function<void(float x, float y, std::unique_ptr<popup_menu>)> cb) {
        show_popup_ = std::move(cb);
    }

private:
    std::vector<menu_item*> items_;
    std::vector<menu_desc> menus_;
    std::function<void(float x, float y, std::unique_ptr<popup_menu>)> show_popup_ = nullptr;
    int active_index_ = -1;

    void show_menu_popup(int index);
};

} 
