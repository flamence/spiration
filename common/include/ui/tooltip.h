/**
 * @file tooltip.h
 * @brief 悬停提示控件，显示简要文本说明。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme.h>
#include <string>

namespace spiration {

/**
 * @brief 悬停提示浮层，由宿主在适当位置显示。
 */
class tooltip : public widget {
public:
    std::string text;
    float font_size = 12.0f;
    float padding = 6.0f;

    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;
    void measure(float& out_w, float& out_h) const;
};

}
