/**
 * @file progress_bar.cpp
 * @brief 进度条控件实现。
 * @author clk
 */

#include <ui/progress_bar.h>
#include <algorithm>

namespace spiration {

void progress_bar::paint(std::shared_ptr<renderer> renderer) {
    float cy = height * 0.5f;
    float by = cy - bar_height * 0.5f;

    renderer->draw_rectangle({0, by, width, bar_height}, bg_color);

    float t = std::max(0.0f, std::min(1.0f, progress));
    float fill_w = width * t;
    if (fill_w > 0.0f) {
        renderer->draw_rectangle({0, by, fill_w, bar_height}, fill_color);
    }
}

size progress_bar::layout_preferred_size() const {
    return {width, bar_height};
}

} // namespace spiration
