/**
 * @file animation.h
 * @brief 属性动画过渡系统，支持颜色、数值的平滑插值。
 * @author clk
 */

#pragma once

#include <ui/color.h>
#include <functional>

namespace spiration {

/**
 * @brief 颜色过渡动画。
 */
class color_transition {
public:
    color_transition() = default;

    /**
     * @brief 构造并设置初始颜色。
     */
    explicit color_transition(const color& initial)
        : current_(initial), from_(initial), target_(initial) {}

    /**
     * @brief 启动向目标颜色的过渡动画。
     * @param target  目标颜色
     * @param duration_ms 动画持续毫秒数
     */
    void animate_to(const color& target, float duration_ms = 120.0f) {
        if (target_ == target) return;
        from_ = current_;
        target_ = target;
        elapsed_ = 0.0f;
        duration_ = duration_ms;
        animating_ = true;
    }

    /**
     * @brief 跳转到目标颜色。
     */
    void snap_to(const color& target) {
        from_ = target;
        current_ = target;
        target_ = target;
        animating_ = false;
        elapsed_ = 0.0f;
    }

    /**
     * @brief 驱动动画向前推进。
     * @param dt_ms 距离上次调用经过的毫秒数
     * @return true 动画仍在进行中
     */
    bool update(float dt_ms) {
        if (!animating_) return false;
        elapsed_ += dt_ms;
        float t = (duration_ > 0.0f) ? (elapsed_ / duration_) : 1.0f;
        if (t >= 1.0f) {
            current_ = target_;
            animating_ = false;
            return false;
        }
        
        float eased = 1.0f - (1.0f - t) * (1.0f - t);
        current_.r = from_.r + (target_.r - from_.r) * eased;
        current_.g = from_.g + (target_.g - from_.g) * eased;
        current_.b = from_.b + (target_.b - from_.b) * eased;
        current_.a = from_.a + (target_.a - from_.a) * eased;
        return true;
    }

    /**
     * @brief 获取当前插值颜色。
     */
    const color& current() const { return current_; }

    /**
     * @brief 是否正在动画中。
     */
    bool is_animating() const { return animating_; }

private:
    color current_;
    color from_;
    color target_;
    float elapsed_ = 0.0f;
    float duration_ = 0.0f;
    bool animating_ = false;
};

} 
