/**
 * @file context_menu.cpp
 * @brief 上下文菜单实现�?
 * @author clk
 */

#include <ui/context_menu.h>
#include <algorithm>

namespace spiration {

void context_menu::add_item(const std::string& text, std::function<void()> callback) {
    items_.push_back({text, std::move(callback), false, true});
    recalc_size();
}

void context_menu::add_separator() {
    items_.push_back({"", nullptr, true, true});
    recalc_size();
}

void context_menu::recalc_size() {
    float h = 0.0f;
    for (auto& it : items_) h += it.separator ? 7.0f : item_height;
    height = h;
    width = min_width;
    for (auto& it : items_) {
        float tw = it.text.size() * 8.0f + 32.0f;
        if (tw > width) width = tw;
    }
}

void context_menu::show_at(float px, float py) {
    x = px; y = py;
    visible_ = true;
    hovered_ = -1;
    hover_bg_.snap_to(color::transparent());
}

void context_menu::dismiss() {
    visible_ = false;
    if (on_dismiss) on_dismiss();
}

bool context_menu::hit_test(float x, float y) const {
    return visible_ && x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void context_menu::tick(float dt_ms) {
    if (visible_ && hover_bg_.update(dt_ms) && request_repaint_) request_repaint_();
    widget::tick(dt_ms);
}

void context_menu::handle_event(const event_type& type, void* data) {
    if (!visible_) return;
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        int idx = -1;
        if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
            float cur = 0.0f;
            for (size_t i = 0; i < items_.size(); ++i) {
                float h = items_[i].separator ? 7.0f : item_height;
                if (my >= cur && my < cur + h) { idx = static_cast<int>(i); break; }
                cur += h;
            }
        }

        if (idx != hovered_) {
            hovered_ = idx;
            if (idx >= 0 && !items_[idx].separator && items_[idx].enabled)
                hover_bg_.animate_to(theme_manager::get(theme_manager::POPUP_HOVER), 100.0f);
            else
                hover_bg_.animate_to(color::transparent(), 100.0f);
            if (request_repaint_) request_repaint_();
        }

        if (md->action == mouse_action::down) {
            if (idx >= 0 && !items_[idx].separator && items_[idx].enabled) {
                md->consumed = true;
                auto cb = items_[idx].callback;
                dismiss();
                if (cb) cb();
                return;
            }
            md->consumed = true;
            dismiss();
            return;
        }
    }
    widget::handle_event(type, data);
}

void context_menu::paint(std::shared_ptr<renderer> renderer) {
    if (!visible_) return;
    renderer->draw_rounded_rectangle({x, y, width, height}, theme_manager::get(theme_manager::POPUP_BG), 4.0f);
    renderer->draw_rounded_rectangle_outline({x, y, width, height}, theme_manager::get(theme_manager::POPUP_BORDER), 4.0f, 1.0f);

    float iy = y;
    for (size_t i = 0; i < items_.size(); ++i) {
        float h = items_[i].separator ? 7.0f : item_height;
        if (items_[i].separator) {
            float cy = iy + h * 0.5f;
            renderer->draw_line({x + 8.0f, cy}, {x + width - 8.0f, cy}, theme_manager::get(theme_manager::SEPARATOR), 1.0f);
        } else {
            if (static_cast<int>(i) == hovered_ && items_[i].enabled) {
                renderer->draw_rectangle({x + 2.0f, iy + 1.0f, width - 4.0f, h - 2.0f}, hover_bg_.current());
            }
            color tc = items_[i].enabled ? theme_manager::get(theme_manager::POPUP_TEXT) : theme_manager::get(theme_manager::TEXT_MUTED);
            renderer->draw_text_aligned(items_[i].text, {x + 14.0f, iy, width - 20.0f, h},
                                        tc, text_alignment::left, vertical_alignment::center, 13.0f);
        }
        iy += h;
    }
}

size context_menu::layout_preferred_size() const {
    return {width, height};
}

}
