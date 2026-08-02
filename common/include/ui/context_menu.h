/**
 * @file context_menu.h
 * @brief 右键上下文菜单，可附加到任意 widget。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme_manager.h>
#include <utils/animation.h>
#include <functional>
#include <string>
#include <vector>

namespace spiration {

struct context_menu_item {
    std::string text;
    std::function<void()> callback;
    bool separator = false;
    bool enabled = true;
};

/**
 * @brief 右键弹出上下文菜单。
 */
class context_menu : public widget {
public:
    context_menu() {
        widget_style.cursor = cursor_type::pointer;
    }

    void add_item(const std::string& text, std::function<void()> callback);
    void add_separator();

    bool hit_test(float x, float y) const override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;

    void show_at(float px, float py);
    void dismiss();
    bool visible() const { return visible_; }

    std::function<void()> on_dismiss;
    float item_height = 28.0f;
    float min_width = 150.0f;

private:
    std::vector<context_menu_item> items_;
    bool visible_ = false;
    int hovered_ = -1;
    color_transition hover_bg_{color::transparent()};
    void recalc_size();
};

}
