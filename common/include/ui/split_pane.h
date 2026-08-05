/**
 * @file split_pane.h
 * @brief 可拖拽分割面板，支持水平或垂直方向的两个子区域。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/theme_manager.h>
#include <functional>

namespace spiration {

/**
 * @brief 可拖拽分割面板，包含两个子容器，中间由拖拽手柄分隔。
 */
class split_pane : public container {
public:
    enum class direction { horizontal, vertical };
    direction dir = direction::vertical;

    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    widget* hit_test_hover(float x, float y) const override;
    cursor_type effective_cursor(float lx, float ly) const override;

    void set_split_ratio(float ratio);
    float split_ratio() const { return split_ratio_; }

    widget* first() const;
    widget* second() const;

    float handle_size = 2.0f;
    float min_ratio = 0.1f;
    float max_ratio = 0.9f;

    /// @brief 拖拽时第一个子区域的最小/最大像素宽度。
    float min_first_px = 0.0f;
    float max_first_px = 0.0f;
    /// @brief 拖拽时第二个子区域的最小像素宽度。
    float min_second_px = 0.0f;

    /// @brief 是否绘制/响应拖拽手柄。
    bool show_handle = true;
    /// @brief 是否由本面板自动布局两个子区域。
    bool auto_layout = true;
    /// @brief 分割比例变化回调。
    std::function<void()> on_ratio_changed;

private:
    float split_ratio_ = 0.5f;
    bool dragging_ = false;
    bool hovering_handle_ = false;
    float drag_start_pos_ = 0.0f;
    float drag_start_ratio_ = 0.5f;

    /** @brief 沿分割方向的位置是否命中手柄拖拽区。 */
    bool handle_hit(float pos) const;
    /** @brief 按像素约束钳制拖拽比例。 */
    float clamp_ratio_for_drag(float r) const;
};

}
