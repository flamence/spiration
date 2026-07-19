/**
 * @file split_pane.h
 * @brief 可拖拽分割面板，支持水平或垂直方向的两个子区域。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/theme.h>

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

    void set_split_ratio(float ratio);
    float split_ratio() const { return split_ratio_; }

    widget* first() const;
    widget* second() const;

    float handle_size = 4.0f;
    float min_ratio = 0.1f;
    float max_ratio = 0.9f;

private:
    float split_ratio_ = 0.5f;
    bool dragging_ = false;
    bool hovering_handle_ = false;
    float drag_start_pos_ = 0.0f;
    float drag_start_ratio_ = 0.5f;
};

}
