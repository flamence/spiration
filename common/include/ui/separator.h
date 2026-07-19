/**
 * @file separator.h
 * @brief 分割线控件，用于视觉上分隔界面区域。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme.h>

namespace spiration {

/**
 * @brief 水平或垂直分割线。
 */
class separator : public widget {
public:
    /**
     * @brief 分割线方向。
     */
    enum class direction {
        horizontal,
        vertical,
    };

    direction dir = direction::horizontal;
    float thickness = 1.0f;
    color line_color = theme::get(theme::SEPARATOR);

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;
};

}
