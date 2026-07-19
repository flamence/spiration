/**
 * @file scroll_view.h
 * @brief 垂直滚动容器，内容超出视口时显示滚动条。
 * @author clk
 */

#pragma once

#include <ui/container.h>
#include <ui/theme.h>

namespace spiration {

/**
 * @brief 垂直可滚动容器，包含一个内容容器和自动显示/隐藏的滚动条。
 */
class scroll_view : public container {
public:
    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;

    float scroll_offset() const { return scroll_offset_; }
    void scroll_to(float y);
    float scroll_bar_width = 8.0f;

private:
    float content_height_ = 0.0f;
    float scroll_offset_ = 0.0f;
    float scroll_max_ = 0.0f;
    bool hovering_scrollbar_ = false;
    bool dragging_scrollbar_ = false;
    float drag_start_y_ = 0.0f;
    float drag_start_offset_ = 0.0f;

    float thumb_height() const;
    float thumb_y() const;
};

}
