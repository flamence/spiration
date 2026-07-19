/**
 * @file tooltip.cpp
 * @brief 悬停提示控件实现。
 * @author clk
 */

#include <ui/tooltip.h>

namespace spiration {

void tooltip::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rounded_rectangle({x, y, width, height}, theme::get(theme::TOOLTIP_BG), 4.0f);
    renderer->draw_text_aligned(text, {x + padding, y, width - padding * 2.0f, height},
                                theme::get(theme::TOOLTIP_TEXT),
                                text_alignment::left, vertical_alignment::center, font_size);
}

size tooltip::layout_preferred_size() const {
    float w, h;
    measure(w, h);
    return {w, h};
}

void tooltip::measure(float& out_w, float& out_h) const {
    out_w = static_cast<float>(text.size()) * font_size * 0.55f + padding * 2.0f + 8.0f;
    out_h = font_size + padding * 2.0f;
}

}
