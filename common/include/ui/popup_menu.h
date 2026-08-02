/**
 * @file popup_menu.h
 * @brief 弹出菜单控件。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <utils/animation.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spiration {

struct popup_item {
    std::string text;
    std::function<void()> callback;
    bool separator = false;
};

/**
 * @brief 弹出菜单，垂直排列可点击项，菜单项宽度自适应文本。
 */
class popup_menu : public container {
public:
    popup_menu() {
        widget_style.cursor = cursor_type::pointer;
    }

    void init() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void tick(float dt_ms) override;

    void add_item(const std::string& text, std::function<void()> callback);
    void add_separator();
    void set_dismiss_callback(std::function<void()> cb) { on_dismiss_ = cb; }

private:
    std::vector<popup_item> items_;
    int hovered_index_ = -1;
    std::function<void()> on_dismiss_ = nullptr;
    float item_height_ = 28.0f;

    color_transition hover_bg_{color::transparent()};

    void layout_items();
};

} 
