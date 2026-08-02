/**
 * @file list_view.h
 * @brief 列表视图，显示可选择的行数据。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme_manager.h>
#include <functional>
#include <string>
#include <vector>

namespace spiration {

/**
 * @brief 简单列表视图。
 */
class list_view : public widget {
public:
    list_view() {
        widget_style.cursor = cursor_type::pointer;
    }

    std::vector<std::string> items;
    int selected_index = -1;
    std::function<void(int)> on_selected;
    float item_height = 28.0f;
    float font_size = 14.0f;

    bool hit_test(float x, float y) const override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;

private:
    int hovered_ = -1;
    float scroll_y_ = 0.0f;
    float content_height_ = 0.0f;
};

}
