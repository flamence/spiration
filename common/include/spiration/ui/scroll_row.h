/**
 * @file scroll_row.h
 * @brief 可水平滚动的行容器，子 widget 定宽排列，超宽时可滚动。
 * @author clk
 */

#pragma once

#include <spiration/ui/container.h>
#include <algorithm>

namespace spiration {

/**
 * @brief 可水平滚动的行容器。
 */
class scroll_row : public container {
public:
    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;

    /**
     * @brief 设置每个子 widget 的固定宽度。
     */
    void set_child_width(float w) { child_width_ = w; }
    float child_width() const { return child_width_; }

    float scroll_offset() const { return scroll_offset_; }

    /// @brief 设置水平滚动偏移（自动夹取到有效范围）。
    void set_scroll_offset(float x) {
        scroll_offset_ = std::max(0.0f, std::min(x, scroll_max_));
    }

    /// @brief 供父级坐标换算/命中测试使用（子控件坐标已含滚动偏移）。
    float scroll_offset_x_for_children() const override { return scroll_offset_; }

    /**
     * @brief 获取内容总宽度。
     */
    float content_width() const { return content_width_; }

private:
    float child_width_ = 0.0f;
    float scroll_offset_ = 0.0f;
    float content_width_ = 0.0f;
    float scroll_max_ = 0.0f;
};

} 
