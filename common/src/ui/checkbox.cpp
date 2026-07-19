/**
 * @file checkbox.cpp
 * @brief 复选框控件实现。
 * @author clk
 */

#include <ui/checkbox.h>

namespace spiration {

bool checkbox::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void checkbox::tick(float dt_ms) {
    if (bg_transition_.update(dt_ms)) {
        if (request_repaint_) request_repaint_();
    }
    widget::tick(dt_ms);
}

void checkbox::on_hover_change(bool hovered) {
    bg_transition_.animate_to(hovered ? hover_color : color::transparent(), 100.0f);
}

void checkbox::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        if (md->action == mouse_action::down && is_hovered()) {
            md->consumed = true;
            checked = !checked;
            if (on_changed) on_changed(checked);
            if (request_repaint_) request_repaint_();
        }
    }
    widget::handle_event(type, data);
}

void checkbox::paint(std::shared_ptr<renderer> renderer) {
    float cy = y + height * 0.5f;
    float bx = x;
    float by = cy - box_size * 0.5f;

    renderer->draw_rectangle({bx, by, box_size, box_size}, bg_transition_.current());
    renderer->draw_rectangle_outline(
        {bx, by, box_size, box_size},
        theme::get(theme::CHECKBOX_BORDER), 1.5f);

    if (checked) {
        renderer->draw_rectangle(
            {bx + 2.0f, by + 2.0f, box_size - 4.0f, box_size - 4.0f},
            theme::get(theme::CHECKBOX_CHECK_BG));
    }

    float tx = bx + box_size + gap;
    renderer->draw_text_aligned(
        text,
        {tx, y, width - (tx - x), height},
        theme::get(theme::LABEL_TEXT),
        text_alignment::left,
        vertical_alignment::center,
        14.0f);
}

size checkbox::layout_preferred_size() const {
    return {width, height};
}

}
