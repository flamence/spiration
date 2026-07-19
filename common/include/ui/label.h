/**
 * @file label.h
 * @brief 文本标签控件，用于显示不可交互的文本。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <string>

namespace spiration {

/**
 * @brief 静态文本标签，显示单行或多行文本。
 */
class label : public widget {
public:
    std::string text;
    float font_size = 14.0f;
    text_alignment h_align = text_alignment::left;
    vertical_alignment v_align = vertical_alignment::center;

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;
};

}
