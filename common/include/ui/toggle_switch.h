/**
 * @file toggle_switch.h
 * @brief 开关控件，支持开/关状态切换，带平滑动画。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme.h>
#include <utils/animation.h>
#include <functional>

namespace spiration {

/**
 * @brief 开关切换控件，带滑块动画。
 */
class toggle_switch : public widget {
public:
    bool active = false;
    std::function<void(bool)> on_changed;

    void init() override {
        knob_pos_.snap_to(active ? 1.0f : 0.0f);
    }

    void set_active(bool v) {
        if (active == v) return;
        active = v;
        knob_pos_.animate_to(active ? 1.0f : 0.0f, 150.0f);
    }

    bool hit_test(float x, float y) const override;

    void tick(float dt_ms) override;

    void handle_event(const event_type& type, void* data) override;

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;

    float track_width = 40.0f;
    float track_height = 22.0f;
    float knob_margin = 3.0f;

private:
    value_transition knob_pos_{0.0f};
    void on_hover_change(bool hovered) override;
};

}
