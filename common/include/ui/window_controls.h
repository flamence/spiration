/**
 * @file window_controls.h
 * @brief 平台感知的窗口控制按钮。
 * @author clk
 */

#pragma once

#include <ui/container.h>

namespace spiration {

/**
 * @brief 窗口控制按钮组。
 */
class window_controls : public container {
public:
    void init() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    bool hit_test(float x, float y) const override;

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
