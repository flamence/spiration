/**
 * @file container.cpp
 * @brief Container 容器控件实现。
 * @author clk
 */

#include <ui/container.h>

namespace spiration {

void container::layout() {
    
    if (layout_manager_) {
        layout_manager_->arrange({0.0f, 0.0f, width, height}, children());
    }
    
    widget::layout();
}

void container::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({x, y, width, height}, style.background_color);
    
    widget::paint(renderer);
}

}