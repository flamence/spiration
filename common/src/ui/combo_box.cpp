/**
 * @file combo_box.cpp
 * @brief 下拉选择框实现。
 * @author clk
 */

#include <ui/combo_box.h>
#include <algorithm>

namespace spiration {

float combo_box::popup_height() const {
    return static_cast<float>(items.size()) * item_height;
}

bool combo_box::hit_test(float x, float y) const {
    if (expanded_) {
        return (x >= 0.0f && x <= width) &&
               (y >= 0.0f && y <= height + popup_height());
    }
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void combo_box::tick(float dt_ms) {
    widget::tick(dt_ms);
}

void combo_box::toggle() { expanded_ = !expanded_; if (request_repaint_) request_repaint_(); }
void combo_box::close() { expanded_ = false; hovered_idx_ = -1; if (request_repaint_) request_repaint_(); }

void combo_box::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        if (md->action == mouse_action::down) {
            if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
                md->consumed = true;
                toggle();
                return;
            }
            if (expanded_ && mx >= 0.0f && mx <= width &&
                my > height && my <= height + popup_height()) {
                md->consumed = true;
                int idx = static_cast<int>((my - height) / item_height);
                if (idx >= 0 && idx < static_cast<int>(items.size())) {
                    selected_index = idx;
                    if (on_changed) on_changed(idx);
                }
                close();
                return;
            }
            close();
        }
        if (md->action == mouse_action::move && expanded_) {
            if (mx >= 0.0f && mx <= width && my > height && my <= height + popup_height()) {
                hovered_idx_ = static_cast<int>((my - height) / item_height);
                if (request_repaint_) request_repaint_();
            }
        }
    }
    widget::handle_event(type, data);
}

void combo_box::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({x, y, width, height}, theme::get(theme::COMBO_BG));
    renderer->draw_rectangle_outline({x, y, width, height}, theme::get(theme::COMBO_BORDER), 1.5f);

    std::string display = (selected_index >= 0 && selected_index < static_cast<int>(items.size()))
                              ? items[selected_index] : "";
    renderer->draw_text_aligned(display, {x + 8.0f, y, width - 24.0f, height},
                                theme::get(theme::INPUT_TEXT),
                                text_alignment::left, vertical_alignment::center, font_size);

    float arrow_cx = x + width - 12.0f;
    float arrow_cy = y + height * 0.5f;
    color arrow_c = theme::get(theme::COMBO_ARROW);
    renderer->draw_line({arrow_cx - 3.0f, arrow_cy - 2.0f}, {arrow_cx, arrow_cy + 2.0f}, arrow_c, 1.5f);
    renderer->draw_line({arrow_cx, arrow_cy + 2.0f}, {arrow_cx + 3.0f, arrow_cy - 2.0f}, arrow_c, 1.5f);

    if (expanded_) {
        float py = y + height;
        renderer->draw_rectangle({x, py, width, popup_height()}, theme::get(theme::POPUP_BG));
        renderer->draw_rectangle_outline({x, py, width, popup_height()}, theme::get(theme::POPUP_BORDER), 1.0f);

        for (size_t i = 0; i < items.size(); ++i) {
            float iy = py + i * item_height;
            if (static_cast<int>(i) == hovered_idx_) {
                renderer->draw_rectangle({x + 1.0f, iy, width - 2.0f, item_height},
                                         theme::get(theme::POPUP_HOVER));
            }
            renderer->draw_text_aligned(items[i], {x + 10.0f, iy, width - 20.0f, item_height},
                                        theme::get(theme::POPUP_TEXT),
                                        text_alignment::left, vertical_alignment::center, font_size);
        }
    }
}

size combo_box::layout_preferred_size() const {
    return {width, height};
}

}
