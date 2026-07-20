/**
 * @file button.h
 * @brief 按钮控件定义。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/theme_manager.h>
#include <utils/animation.h>

namespace spiration {

/**
 * @brief 可交互的按钮控件。
 */
class button : public container {
private:
    bool pressing_ = false;

protected:
    
    color_transition bg_transition_{color::transparent()};
    void on_hover_change(bool hovered) override;

public:
    std::string text;

    bool hit_test(float x, float y) const override;

    void tick(float dt_ms) override;

    void handle_event(const event_type& type, void* data) override;

    void paint(std::shared_ptr<renderer> renderer) override;

    /**
     * @brief hover 状态背景色。
     */
    color hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    color press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
};

}