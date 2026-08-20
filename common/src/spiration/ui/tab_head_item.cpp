/**
 * @file tab_head_item.cpp
 * @brief 标签头项控件实现。
 * @author clk
 */

#include <spiration/ui/tab_head_item.h>
#include <spiration/ui/focus_manager.h>
#include <spiration/ui/theme_manager.h>

namespace spiration {

tab_head_item::tab_head_item(const std::string& title)
    : title_(title)
    , bg_(theme_manager::get(theme_manager::TAB_INACTIVE_BG))
    , text_(theme_manager::get(theme_manager::TAB_INACTIVE_TEXT))
    , close_fg_(theme_manager::get(theme_manager::TAB_CLOSE_FG)) {
    widget_style.cursor = cursor_style::pointer;
    focusable = true;
}

void tab_head_item::sync_colors() {
    if (active_) {
        bg_.animate_to(theme_manager::get(theme_manager::TAB_ACTIVE_BG), 150.0f);
        text_.animate_to(theme_manager::get(theme_manager::TAB_ACTIVE_TEXT), 150.0f);
    } else if (hovering_) {
        bg_.animate_to(theme_manager::get(theme_manager::TAB_HOVER_BG), 100.0f);
        text_.animate_to(theme_manager::get(theme_manager::TAB_HOVER_TEXT), 100.0f);
    } else {
        bg_.animate_to(theme_manager::get(theme_manager::TAB_INACTIVE_BG), 100.0f);
        text_.animate_to(theme_manager::get(theme_manager::TAB_INACTIVE_TEXT), 100.0f);
    }
}

void tab_head_item::tick(float dt_ms) {
    bool need = false;
    if (bg_.update(dt_ms)) need = true;
    if (text_.update(dt_ms)) need = true;
    if (close_fg_.update(dt_ms)) need = true;
    if (need && request_repaint_) request_repaint_();
}

void tab_head_item::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        bool old_hover = hovering_;
        bool old_close = close_hovering_;

        hovering_ = (md->position.x >= 0.0f && md->position.x < width &&
                     md->position.y >= 0.0f && md->position.y < height);
        close_hovering_ = (hovering_ &&
                           md->position.x >= width - CLOSE_BTN_W &&
                           md->position.x < width);

        if (hovering_ != old_hover) {
            sync_colors();
            if (request_repaint_) request_repaint_();
        }

        if (close_hovering_ != old_close) {
            if (close_hovering_) {
                close_fg_.animate_to(theme_manager::get(theme_manager::TAB_CLOSE_HOVER_FG), 100.0f);
            } else {
                close_fg_.animate_to(theme_manager::get(theme_manager::TAB_CLOSE_FG), 100.0f);
            }
            if (request_repaint_) request_repaint_();
        }

        if (md->action == mouse_action::down) {
            if (close_hovering_) {
                md->consumed = true;
                if (on_close_) on_close_();
                return;
            }
            if (hovering_) {
                md->consumed = true;
                focus_manager::instance().request_focus(this);
                if (on_activate_) on_activate_();
                return;
            }
        }
        return;
    }
    widget::handle_event(type, data);
}

void tab_head_item::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({0, 0, width, height}, bg_.current());

    float avail_w = width - CLOSE_BTN_W - 14.0f;
    std::string display = title_;
    float text_w = renderer->measure_text_width(display, 13.0f);
    if (text_w > avail_w && !display.empty()) {
        std::string dots = "...";
        float dots_w = renderer->measure_text_width(dots, 13.0f);
        float max_w = avail_w - dots_w;
        while (!display.empty() && renderer->measure_text_width(display, 13.0f) > max_w) {
            display.pop_back();
            while (!display.empty() &&
                   (static_cast<unsigned char>(display.back()) & 0xC0) == 0x80) {
                display.pop_back();
            }
        }
        display += dots;
    }

    renderer->draw_text_aligned(display,
                                {10.0f, 0, avail_w, height},
                                text_.current(),
                                text_alignment::left, vertical_alignment::center, 13.0f);

    float centerX = width - CLOSE_BTN_W * 0.5f;
    float centerY = height * 0.5f;
    float half = CLOSE_ICON_SIZE * 0.5f;
    color closeCol = close_fg_.current();
    renderer->draw_line({centerX - half, centerY - half},
                        {centerX + half, centerY + half}, closeCol, 1.5f);
    renderer->draw_line({centerX + half, centerY - half},
                        {centerX - half, centerY + half}, closeCol, 1.5f);
}

} // namespace spiration
