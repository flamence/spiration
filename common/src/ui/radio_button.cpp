/**
 * @file radio_button.cpp
 * @brief 单选按钮实现。
 * @author clk
 */

#include <ui/radio_button.h>
#include <ui/focus_manager.h>

namespace spiration {

bool radio_button::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void radio_button::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        hovering_ = (md->position.x >= 0.0f && md->position.x <= width &&
                     md->position.y >= 0.0f && md->position.y <= height);
        if (md->action == mouse_action::down && hovering_) {
            md->consumed = true;
            focus_manager::instance().request_focus(this);
            if (!selected) {
                selected = true;
                if (on_changed) on_changed(true);
                if (request_repaint_) request_repaint_();
            }
        }
    }
    widget::handle_event(type, data);
}

void radio_button::paint(std::shared_ptr<renderer> renderer) {
    float cy = height * 0.5f;
    float cx = radius;
    float outer = radius;
    float inner = radius * 0.55f;

    renderer->draw_circle_outline({cx, cy}, outer, theme_manager::get(theme_manager::CHECKBOX_BORDER), 1.5f);
    if (selected) {
        renderer->draw_circle({cx, cy}, inner, theme_manager::get(theme_manager::CHECKBOX_CHECK_BG));
    }

    float tx = cx + outer + gap;
    renderer->draw_text_aligned(text, {tx, 0, width - tx, height},
                                theme_manager::get(theme_manager::LABEL_TEXT),
                                text_alignment::left, vertical_alignment::center, 14.0f);
}

size radio_button::layout_preferred_size() const {
    return {width, height};
}

} // namespace spiration
