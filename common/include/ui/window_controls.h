/**
 * @file window_controls.h
 * @brief 平台感知的窗口控制按钮（最小化/最大化/关闭）。
 * @author clk
 */

#pragma once

#include <ui/container.h>

namespace spiration {

/**
 * @brief 窗口控制按钮组。
 *
 * 自动检测平台：
 * - Windows/Linux：右侧排列 [—] [□] [×]
 * - macOS：左侧排列 [●] [●] [●]（红绿灯）
 *
 * 点击按钮通过 window_action_ 回调通知窗口执行对应操作。
 */
class window_controls : public container {
public:
    void init() override;
    void paint(std::shared_ptr<renderer> renderer) override;

    void set_control_size(float size) { btn_size_ = size; }

private:
    float btn_size_ = 46.0f;  
    float btn_h_ = 34.0f;

    
    bool hover_min_ = false, hover_max_ = false, hover_close_ = false;

    void handle_event(const event_type& type, void* data) override;

    enum class hit_part { none, min, max, close };
    hit_part hit_test_btn(float mx, float my) const;
};

} 
