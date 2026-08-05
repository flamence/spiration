/**
 * @file list_view.cpp
 * @brief 列表视图实现。
 * @author clk
 */

#include <ui/list_view.h>
#include <ui/focus_manager.h>
#include <algorithm>

namespace spiration {

bool list_view::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void list_view::tick(float dt_ms) {
    widget::tick(dt_ms);
}

void list_view::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        content_height_ = items.size() * item_height;
        float scroll_max = std::max(0.0f, content_height_ - height);

        if (md->action == mouse_action::wheel) {
            scroll_y_ = std::max(0.0f, std::min(scroll_y_ + (md->wheel_delta > 0 ? -30.0f : 30.0f), scroll_max));
            md->consumed = true;
            if (request_repaint_) request_repaint_();
            return;
        }

        if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
            if (md->action == mouse_action::move) {
                hovered_ = static_cast<int>((my + scroll_y_) / item_height);
                if (hovered_ >= static_cast<int>(items.size())) hovered_ = -1;
                if (request_repaint_) request_repaint_();
            }
            if (md->action == mouse_action::down) {
                int idx = static_cast<int>((my + scroll_y_) / item_height);
                if (idx >= 0 && idx < static_cast<int>(items.size())) {
                    selected_index = idx;
                    md->consumed = true;
                    focus_manager::instance().request_focus(this);
                    if (on_selected) on_selected(idx);
                    if (request_repaint_) request_repaint_();
                }
            }
        }
    }
    widget::handle_event(type, data);
}

void list_view::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({0, 0, width, height}, theme_manager::get(theme_manager::LIST_BG));
    renderer->push_clip({0, 0, width, height});

    float iy = -scroll_y_;
    for (size_t i = 0; i < items.size(); ++i) {
        if (iy + item_height < 0 || iy > height) { iy += item_height; continue; }
        color bg = color::transparent();
        if (static_cast<int>(i) == selected_index)
            bg = theme_manager::get(theme_manager::LIST_ITEM_SELECTED);
        else if (static_cast<int>(i) == hovered_)
            bg = theme_manager::get(theme_manager::LIST_ITEM_HOVER);

        if (bg.a > 0.0f) renderer->draw_rectangle({0, iy, width, item_height}, bg);
        renderer->draw_text_aligned(items[i], {10.0f, iy, width - 20.0f, item_height},
                                    theme_manager::get(theme_manager::LABEL_TEXT),
                                    text_alignment::left, vertical_alignment::center, font_size);
        iy += item_height;
    }

    renderer->pop_clip();

    content_height_ = items.size() * item_height;
    float scroll_max = std::max(0.0f, content_height_ - height);
    if (scroll_max > 0.0f) {
        float sb_w = 6.0f;
        float sb_x = width - sb_w;
        float th = std::max(20.0f, height * (height / content_height_));
        float ty = (scroll_y_ / scroll_max) * (height - th);
        renderer->draw_rectangle({sb_x, 0, sb_w, height}, theme_manager::get(theme_manager::SCROLL_BAR_BG));
        renderer->draw_rounded_rectangle({sb_x + 1.0f, ty, sb_w - 2.0f, th},
                                         theme_manager::get(theme_manager::SCROLL_BAR_THUMB), 3.0f);
    }
}

size list_view::layout_preferred_size() const {
    return {width, height};
}

} // namespace spiration
