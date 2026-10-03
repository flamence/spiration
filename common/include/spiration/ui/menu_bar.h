/**
 * @file menu_bar.h
 * @brief 菜单栏控件。
 * @author clk
 */

#pragma once

#include <spiration/ui/container.h>
#include <spiration/ui/button.h>
#include <spiration/ui/popup_menu.h>
#include <spiration/ui/theme_manager.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

/**
 * @brief 菜单项目。
 */
class menu_item : public button {
public:
    void paint(std::shared_ptr<renderer> renderer) override {
        renderer->draw_rectangle({0, 0, width, height}, bg_transition_.current());
        renderer->draw_text_aligned(text, rectangle{ 0, 0, width, height }, theme_manager::get(theme_manager::MENU_TEXT),
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
        std::string owner;
    };
    std::vector<item> sub_items;

    /// @brief 无尾项的哨兵值。
    static constexpr size_t NO_TAIL = static_cast<size_t>(-1);
    /// @brief 固定末尾子项索引，后续添加的子项插到其前。
    size_t tail_index = NO_TAIL;

    void add_separator() { sub_items.push_back({"", nullptr, true, ""}); }
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
     * @param text 菜单标题。
     * @return menu_desc& 引用，用于添加子项。
     */
    menu_desc& add_menu(const std::string& text);

    /**
     * @brief 在已存在的菜单中添加子项。
     * @param menu_title 目标菜单标题。
     * @param label 子项标签。
     * @param callback 点击回调。
     * @param owner 归属者标识（拓展 ID），空串表示宿主内置。
     * @return true 添加成功，false 未找到目标菜单。
     */
    bool add_sub_item(const std::string& menu_title,
                      const std::string& label,
                      std::function<void()> callback,
                      const std::string& owner = std::string());

    /**
     * @brief 移除某个归属者的全部子项。
     * @param owner 归属者标识。
     * @return 移除的子项数量。
     * @note 若某个菜单因此变空，则连同其控件一并移除。
     */
    size_t remove_owned_items(const std::string& owner);

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
