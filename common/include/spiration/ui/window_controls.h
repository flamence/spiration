/**
 * @file window_controls.h
 * @brief 平台感知的窗口控制按钮。
 * @author clk
 */

#pragma once

#include <spiration/ui/container.h>
#include <spiration/utils/animation.h>

namespace spiration {

#if !defined(__APPLE__)
/**
 * @brief 窗口控制按钮组。
 */
class window_controls : public container {
public:
    window_controls() {
        widget_style.cursor = cursor_style::pointer;
    }

    void init() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void tick(float dt_ms) override;
    bool hit_test(float x, float y) const override;

    void set_control_size(float size) { btn_size_ = size; }

private:
    float btn_size_ = 46.0f;  
    float btn_h_ = 34.0f;

    
    bool hover_min_ = false, hover_max_ = false, hover_close_ = false;

    /// @brief 最小化按钮悬停背景过渡。
    color_transition hover_min_bg_{color::transparent()};
    /// @brief 最大化按钮悬停背景过渡。
    color_transition hover_max_bg_{color::transparent()};
    /// @brief 关闭按钮悬停背景过渡。
    color_transition close_bg_{color::transparent()};

    void handle_event(const event_type& type, void* data) override;

    enum class hit_part { none, min, max, close };
    hit_part hit_test_btn(float mx, float my) const;
};
#endif

} 
