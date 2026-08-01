/**
 * @file container.h
 * @brief Container 容器控件，可容纳子 widget 并绘制背景。
 * @author clk
 */

#pragma once

#include "widget.h"
#include <ui/layout.h>
#include <utils/animation.h>
#include <memory>

namespace spiration {

/**
 * @brief 容器控件。
 */
class container : public widget {
public:
    void layout() override;

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;

    void tick(float dt_ms) override;

    void handle_event(const event_type& type, void* data) override;

    /**
     * @brief 设置布局管理器。
     */
    void set_layout_manager(std::unique_ptr<layout_manager> lm) {
        layout_manager_ = std::move(lm);
    }

    /**
     * @brief 获取当前布局管理器。
     */
    layout_manager* get_layout_manager() const { return layout_manager_.get(); }

    /**
     * @brief 滚动条宽度。
     */
    float scroll_bar_width = 14.0f;

    /**
     * @brief 当前水平滚动偏移。
     */
    float scroll_offset_x() const { return scroll_x_; }
    /**
     * @brief 当前垂直滚动偏移。
     */
    float scroll_offset_y() const { return scroll_y_; }
    /**
     * @brief 水平最大滚动量。
     */
    float scroll_max_x() const { return scroll_max_x_; }
    /**
     * @brief 垂直最大滚动量。
     */
    float scroll_max_y() const { return scroll_max_y_; }

    /**
     * @brief 水平滚动到指定偏移。
     */
    void scroll_to_x(float x);
    /**
     * @brief 垂直滚动到指定偏移。
     */
    void scroll_to_y(float y);
    /**
     * @brief 同时滚动两个方向。
     */
    void scroll_to(float x, float y);

    /**
     * @brief 供单控件容器声明内容尺寸并夹取滚动。
     */
    void set_scroll_content(float content_w, float content_h);

    /**
     * @brief 绘制垂直/水平滚动条，仅限单控件内容。
     */
    void draw_scrollbars(std::shared_ptr<renderer> renderer);

    float scroll_offset_for_children() const override { return scroll_y_; }
    float scroll_offset_x_for_children() const override { return scroll_x_; }

private:
    std::unique_ptr<layout_manager> layout_manager_ = nullptr;

    float content_width_ = 0.0f;
    float content_height_ = 0.0f;
    float scroll_x_ = 0.0f;
    float scroll_y_ = 0.0f;
    float scroll_max_x_ = 0.0f;
    float scroll_max_y_ = 0.0f;
    bool hovering_v_ = false;
    bool dragging_v_ = false;
    float drag_start_y_ = 0.0f;
    float drag_start_v_ = 0.0f;
    bool hovering_h_ = false;
    bool dragging_h_ = false;
    float drag_start_x_ = 0.0f;
    float drag_start_h_ = 0.0f;

    color_transition thumb_v_bg_{{0.45f, 0.45f, 0.45f, 0.5f}};
    color_transition thumb_h_bg_{{0.45f, 0.45f, 0.45f, 0.5f}};

    float v_thumb_height() const;
    float v_thumb_y() const;
    float h_thumb_width() const;
    float h_thumb_x() const;
};

}