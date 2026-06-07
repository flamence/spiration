/**
 * @file popup_menu.h
 * @brief 弹出菜单控件。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

struct popup_item {
    std::string text;
    std::function<void()> callback;
};

/**
 * @brief 弹出菜单，垂直排列可点击项，菜单项宽度自适应文本。
 */
class popup_menu : public container {
public:
    void init() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;

    void add_item(const std::string& text, std::function<void()> callback);
    void set_dismiss_callback(std::function<void()> cb) { on_dismiss_ = cb; }

private:
    std::vector<popup_item> items_;
    int hovered_index_ = -1;
    std::function<void()> on_dismiss_ = nullptr;
    float item_height_ = 26.0f;

    void layout_items();
};

} 
