/**
 * @file button.h
 * @brief 按钮控件定义。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/theme.h>
#include <utils/animation.h>

namespace spiration {

/**
 * @brief 可交互的按钮控件。
 *
 * 支持鼠标悬停高亮、按下状态、文本显示，背景色带有平滑过渡动画。
 * 通过 handle_event 响应鼠标事件并更新视觉状态。
 */
class button : public container {
private:
    bool hovering_ = false;
    bool pressing_ = false;

protected:
    
    color_transition bg_transition_{color::transparent()};

public:
    std::string text;

    bool hit_test(float x, float y) const override;

    void tick(float dt_ms) override;

    void handle_event(const event_type& type, void* data) override;

    void paint(std::shared_ptr<renderer> renderer) override;

    /**
     * @brief hover 状态背景色。
     */
    color hover_color = theme::button_hover();
    color press_color = theme::button_press();
};

}