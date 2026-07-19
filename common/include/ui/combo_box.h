/**
 * @file combo_box.h
 * @brief 下拉选择框，点击展开选项列表。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme.h>
#include <functional>
#include <string>
#include <vector>

namespace spiration {

/**
 * @brief 下拉选择框控件。
 */
class combo_box : public widget {
public:
    std::vector<std::string> items;
    int selected_index = -1;
    std::function<void(int)> on_changed;

    bool hit_test(float x, float y) const override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;

    float item_height = 24.0f;
    float font_size = 14.0f;

private:
    bool expanded_ = false;
    bool hovering_ = false;
    int hovered_idx_ = -1;
    float popup_height() const;

    void toggle();
    void close();
};

}
