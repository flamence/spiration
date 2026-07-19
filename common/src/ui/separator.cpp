/**
 * @file separator.cpp
 * @brief 分割线控件实现。
 * @author clk
 */

#include <ui/separator.h>

namespace spiration {

void separator::paint(std::shared_ptr<renderer> renderer) {
    if (dir == direction::horizontal) {
        float cy = y + height * 0.5f;
        renderer->draw_line({x, cy}, {x + width, cy}, line_color, thickness);
    } else {
        float cx = x + width * 0.5f;
        renderer->draw_line({cx, y}, {cx, y + height}, line_color, thickness);
    }
}

size separator::layout_preferred_size() const {
    if (dir == direction::horizontal)
        return {width, thickness};
    else
        return {thickness, height};
}

}
