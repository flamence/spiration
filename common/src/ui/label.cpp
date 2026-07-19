/**
 * @file label.cpp
 * @brief 文本标签控件实现。
 * @author clk
 */

#include <ui/label.h>
#include <ui/theme.h>

namespace spiration {

void label::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_text_aligned(
        text,
        {x, y, width, height},
        theme::get(theme::LABEL_TEXT),
        h_align,
        v_align,
        font_size);
}

size label::layout_preferred_size() const {
    return {width, height};
}

}
