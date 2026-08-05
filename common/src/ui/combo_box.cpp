/**
 * @file combo_box.cpp
 * @brief 下拉选择框实现。
 * @author clk
 */

#include <ui/combo_box.h>
#include <ui/focus_manager.h>
#include <ui/root.h>
#include <algorithm>

namespace spiration {

float combo_box::popup_height() const {
    return static_cast<float>(items.size()) * item_height;
}

float combo_box::effective_popup_height() const {
    float ph = popup_height();
    if (popup_up) {
        float top = to_screen(0.0f, 0.0f).y;
        if (top > 0.0f) ph = std::min(ph, top);
    }
    return ph;
}

bool combo_box::hit_test(float x, float y) const {
    if (expanded_) {
        float ph = effective_popup_height();
        if (popup_up) {
            return (x >= 0.0f && x <= width) && (y >= -ph && y <= height);
        }
        return (x >= 0.0f && x <= width) && (y >= 0.0f && y <= height + ph);
    }
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void combo_box::tick(float dt_ms) {
    if (hover_bg_.update(dt_ms) && request_repaint_) request_repaint_();
    widget::tick(dt_ms);
}

void combo_box::toggle() {
    expanded_ = !expanded_;
    if (expanded_) {
        focus_manager::instance().request_focus(this);
        if (auto* r = find_root()) r->add_overlay(this);
    } else {
        if (auto* r = find_root()) r->remove_overlay(this);
    }
    if (request_repaint_) request_repaint_();
}
void combo_box::close() {
    expanded_ = false;
    hovered_idx_ = -1;
    hover_bg_.snap_to(color::transparent());
    if (auto* r = find_root()) r->remove_overlay(this);
    if (request_repaint_) request_repaint_();
}

void combo_box::on_blur() {
    close();
}

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
            if (expanded_) {
                float ph = effective_popup_height();
                if (popup_up) {
                    if (mx >= 0.0f && mx <= width && my >= -ph && my < 0.0f) {
                        md->consumed = true;
                        int idx = static_cast<int>((my + ph) / item_height);
                        if (idx >= 0 && idx < static_cast<int>(items.size())) {
                            selected_index = idx;
                            if (on_changed) on_changed(idx);
                        }
                        close();
                        return;
                    }
                } else if (mx >= 0.0f && mx <= width &&
                           my > height && my <= height + ph) {
                    md->consumed = true;
                    int idx = static_cast<int>((my - height) / item_height);
                    if (idx >= 0 && idx < static_cast<int>(items.size())) {
                        selected_index = idx;
                        if (on_changed) on_changed(idx);
                    }
                    close();
                    return;
                }
            }
            close();
        }
        if (md->action == mouse_action::move && expanded_) {
            float ph = effective_popup_height();
            int new_hover = -1;
            if (popup_up) {
                if (mx >= 0.0f && mx <= width && my >= -ph && my < 0.0f) {
                    new_hover = static_cast<int>((my + ph) / item_height);
                }
            } else if (mx >= 0.0f && mx <= width && my > height && my <= height + ph) {
                new_hover = static_cast<int>((my - height) / item_height);
            }
            if (new_hover != hovered_idx_) {
                hovered_idx_ = new_hover;
                hover_bg_.animate_to(
                    new_hover >= 0 ? theme_manager::get(theme_manager::POPUP_HOVER)
                                   : color::transparent(),
                    100.0f);
                if (request_repaint_) request_repaint_();
            }
        }
    }
    widget::handle_event(type, data);
}

void combo_box::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({0, 0, width, height}, theme_manager::get(theme_manager::COMBO_BG));
    color border = focused_ ? theme_manager::get(theme_manager::INPUT_FOCUS_BORDER)
                            : theme_manager::get(theme_manager::COMBO_BORDER);
    renderer->draw_rectangle_outline({0, 0, width, height}, border, 1.5f);

    std::string display = (selected_index >= 0 && selected_index < static_cast<int>(items.size()))
                              ? items[selected_index] : "";
    float avail = std::max(0.0f, width - 24.0f);
    if (!display.empty() && avail > 0.0f) {
        float tw = renderer->measure_text_width(display, font_size);
        if (tw > avail) {
            const std::string dots = "...";
            float dots_w = renderer->measure_text_width(dots, font_size);
            float max_w = avail - dots_w;
            if (max_w <= 0.0f) {
                display.clear();
            } else {
                while (!display.empty() &&
                       renderer->measure_text_width(display, font_size) > max_w) {
                    display.pop_back();
                    while (!display.empty() &&
                           (static_cast<unsigned char>(display.back()) & 0xC0) == 0x80)
                        display.pop_back();
                }
                display += dots;
            }
        }
    }
    renderer->draw_text_aligned(display, {8.0f, 0, std::max(0.0f, width - 24.0f), height},
                                theme_manager::get(theme_manager::INPUT_TEXT),
                                text_alignment::left, vertical_alignment::center, font_size);

    float arrow_cx = width - 12.0f;
    float arrow_cy = height * 0.5f;
    color arrow_c = theme_manager::get(theme_manager::COMBO_ARROW);
    renderer->draw_line({arrow_cx - 3.0f, arrow_cy - 2.0f}, {arrow_cx, arrow_cy + 2.0f}, arrow_c, 1.5f);
    renderer->draw_line({arrow_cx, arrow_cy + 2.0f}, {arrow_cx + 3.0f, arrow_cy - 2.0f}, arrow_c, 1.5f);

    if (expanded_) {
        float ph = effective_popup_height();
        float py = popup_up ? -ph : height;
        renderer->draw_rectangle({0, py, width, ph}, theme_manager::get(theme_manager::POPUP_BG));
        renderer->draw_rectangle_outline({0, py, width, ph}, theme_manager::get(theme_manager::POPUP_BORDER), 1.0f);

        for (size_t i = 0; i < items.size(); ++i) {
            float iy = py + static_cast<float>(i) * item_height;
            if (popup_up) {
                if (iy >= 0.0f) break;
            } else if (iy >= py + ph) {
                break;
            }
            if (static_cast<int>(i) == hovered_idx_) {
                renderer->draw_rectangle({1.0f, iy, width - 2.0f, item_height},
                                         hover_bg_.current());
            }
            renderer->draw_text_aligned(items[i], {10.0f, iy, width - 20.0f, item_height},
                                        theme_manager::get(theme_manager::POPUP_TEXT),
                                        text_alignment::left, vertical_alignment::center, font_size);
        }
    }
}

size combo_box::layout_preferred_size() const {
    return {width, height};
}

} // namespace spiration
