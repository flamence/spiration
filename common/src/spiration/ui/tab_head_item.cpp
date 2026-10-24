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

void tab_head_item::set_active(bool active) {
    if (active_ == active) return;
    active_ = active;
    sync_colors();
}

void tab_head_item::set_title(const std::string& t) {
    if (title_ == t) return;
    title_ = t;
    display_key_title_.clear();
    if (request_repaint_) request_repaint_();
}

void tab_head_item::sync_colors() {
    if (active_) {
        bg_.animate_to(theme_manager::get(theme_manager::TAB_ACTIVE_BG), 150.0f);
        text_.animate_to(theme_manager::get(theme_manager::TAB_ACTIVE_TEXT), 150.0f);
    } else if (is_hovered()) {
        bg_.animate_to(theme_manager::get(theme_manager::TAB_HOVER_BG), 100.0f);
        text_.animate_to(theme_manager::get(theme_manager::TAB_HOVER_TEXT), 100.0f);
    } else {
        bg_.animate_to(theme_manager::get(theme_manager::TAB_INACTIVE_BG), 100.0f);
        text_.animate_to(theme_manager::get(theme_manager::TAB_INACTIVE_TEXT), 100.0f);
    }
}

void tab_head_item::on_hover_change(bool hovered) {
    if (!hovered) close_hovering_ = false;
    sync_colors();
    if (request_repaint_) request_repaint_();
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
        const bool inside = (md->position.x >= 0.0f && md->position.x < width &&
                             md->position.y >= 0.0f && md->position.y < height);
        const bool over_close = inside && md->position.x >= width - CLOSE_BTN_W;

        if (over_close != close_hovering_) {
            close_hovering_ = over_close;
            close_fg_.animate_to(theme_manager::get(
                                     over_close ? theme_manager::TAB_CLOSE_HOVER_FG
                                                : theme_manager::TAB_CLOSE_FG),
                                 100.0f);
            if (request_repaint_) request_repaint_();
        }

        if (md->action == mouse_action::down) {
            if (md->button == mouse_button::right && inside) {
                md->consumed = true;
                if (on_context_menu) {
                    point sp = to_screen(md->position.x, md->position.y);
                    on_context_menu(sp.x, sp.y);
                }
                return;
            }
            if (md->button == mouse_button::middle && inside) {
                md->consumed = true;
                if (on_close_) on_close_();
                return;
            }
            if (md->button == mouse_button::left && inside) {
                md->consumed = true;
                if (close_hovering_) {
                    if (on_close_) on_close_();
                    return;
                }
                focus_manager::instance().request_focus(this);
                if (on_activate_) on_activate_();
                return;
            }
        }
        widget::handle_event(type, data);
        return;
    }
    widget::handle_event(type, data);
}

void tab_head_item::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({0, 0, width, height}, bg_.current());

    const float avail_w = width - CLOSE_BTN_W - 14.0f;
    if (display_key_title_ != title_ || display_key_width_ != avail_w) {
        display_key_title_ = title_;
        display_key_width_ = avail_w;
        display_title_ = title_;
        if (renderer->measure_text_width(display_title_, 13.0f) > avail_w && !display_title_.empty()) {
            const float dots_w = renderer->measure_text_width("...", 13.0f);
            const float max_w = avail_w - dots_w;
            while (!display_title_.empty() &&
                   renderer->measure_text_width(display_title_, 13.0f) > max_w) {
                display_title_.pop_back();
                while (!display_title_.empty() &&
                       (static_cast<unsigned char>(display_title_.back()) & 0xC0) == 0x80) {
                    display_title_.pop_back();
                }
            }
            display_title_ += "...";
        }
    }

    renderer->draw_text_aligned(display_title_,
                                {10.0f, 0, avail_w, height},
                                text_.current(),
                                text_alignment::left, vertical_alignment::center, 13.0f);

    // 关闭按钮：活动标签常显，非活动标签悬停时显示（与浏览器惯例一致）。
    if (!active_ && !is_hovered()) return;
    const float centerX = width - CLOSE_BTN_W * 0.5f;
    const float centerY = height * 0.5f;
    const float half = CLOSE_ICON_SIZE * 0.5f;
    const color closeCol = close_fg_.current();
    renderer->draw_line({centerX - half, centerY - half},
                        {centerX + half, centerY + half}, closeCol, 1.5f);
    renderer->draw_line({centerX + half, centerY - half},
                        {centerX - half, centerY + half}, closeCol, 1.5f);
}

} // namespace spiration
