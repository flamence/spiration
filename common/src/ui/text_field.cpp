/**
 * @file text_field.cpp
 * @brief 单行文本输入框实现（仿 edit_tab：选择、精确光标、复制粘贴）。
 * @author clk
 */

#include <ui/text_field.h>
#include <ui/context_menu.h>
#include <extension/builtin/i18n/i18n.h>
#include <window/event.h>
#include <utils/clipboard.h>
#include <algorithm>

namespace spiration {

bool text_field::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void text_field::focus() {
    focused_ = true;
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    if (request_repaint_) request_repaint_();
}

void text_field::blur() {
    focused_ = false;
    selecting_ = false;
    cursor_visible_ = false;
    if (request_repaint_) request_repaint_();
}

void text_field::tick(float dt_ms) {
    if (focused_) {
        cursor_blink_ += dt_ms;
        if (cursor_blink_ >= cursor_blink_interval_) {
            cursor_blink_ -= cursor_blink_interval_;
            cursor_visible_ = !cursor_visible_;
            if (request_repaint_) request_repaint_();
        }
    }
    widget::tick(dt_ms);
}

bool text_field::has_selection() const {
    return selecting_ && sel_anchor_ != cursor_pos_;
}

void text_field::delete_selection() {
    if (!has_selection()) return;
    size_t start = std::min(sel_anchor_, cursor_pos_);
    size_t end   = std::max(sel_anchor_, cursor_pos_);
    text.erase(start, end - start);
    cursor_pos_ = start;
    sel_anchor_ = start;
    selecting_ = false;
    if (on_changed) on_changed(text);
    if (request_repaint_) request_repaint_();
}

std::string text_field::selected_text() const {
    if (!has_selection()) return {};
    size_t start = std::min(sel_anchor_, cursor_pos_);
    size_t end   = std::max(sel_anchor_, cursor_pos_);
    return text.substr(start, end - start);
}

void text_field::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        if (md->action == mouse_action::down && md->button == mouse_button::right) {
            if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
                open_context_menu(mx, my);
                md->consumed = true;
            }
            return;
        }

        if (md->action == mouse_action::down) {
            if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
                md->consumed = true;
                focus();
                mouse_down_ = true;
                selecting_ = false;
                sel_anchor_ = hit_to_cursor(mx, cached_renderer_);
                cursor_pos_ = sel_anchor_;
                ensure_cursor_visible();
                cursor_blink_ = 0.0f;
                cursor_visible_ = true;
                if (request_repaint_) request_repaint_();
            } else {
                blur();
            }
        } else if (md->action == mouse_action::move && mouse_down_) {
            cursor_pos_ = hit_to_cursor(mx, cached_renderer_);
            selecting_ = (cursor_pos_ != sel_anchor_);
            cursor_blink_ = 0.0f;
            cursor_visible_ = true;
            if (request_repaint_) request_repaint_();
        } else if (md->action == mouse_action::up) {
            mouse_down_ = false;
        }

    } else if (type == event_type::keyboard && focused_) {
        auto* kd = static_cast<key_event_data*>(data);

        bool shift = kd->shift;

        if (kd->codepoint >= 32 && kd->codepoint != 127) {
            if (has_selection()) delete_selection();
            std::string s;
            if (kd->codepoint <= 0x7F) {
                s += static_cast<char>(kd->codepoint);
            } else if (kd->codepoint <= 0x7FF) {
                s += static_cast<char>(0xC0 | (kd->codepoint >> 6));
                s += static_cast<char>(0x80 | (kd->codepoint & 0x3F));
            } else if (kd->codepoint <= 0xFFFF) {
                s += static_cast<char>(0xE0 | (kd->codepoint >> 12));
                s += static_cast<char>(0x80 | ((kd->codepoint >> 6) & 0x3F));
                s += static_cast<char>(0x80 | (kd->codepoint & 0x3F));
            } else {
                s += static_cast<char>(0xF0 | (kd->codepoint >> 18));
                s += static_cast<char>(0x80 | ((kd->codepoint >> 12) & 0x3F));
                s += static_cast<char>(0x80 | ((kd->codepoint >> 6) & 0x3F));
                s += static_cast<char>(0x80 | (kd->codepoint & 0x3F));
            }
            insert_at_cursor(s);
            kd->consumed = true;
        } else {
            switch (kd->key_code) {
            case 8:  // Backspace
                if (has_selection()) { delete_selection(); }
                else { delete_before_cursor(); }
                kd->consumed = true;
                break;
            case 46: // Delete
                if (has_selection()) { delete_selection(); }
                else { delete_after_cursor(); }
                kd->consumed = true;
                break;
            case 37: // Left
                if (shift && !selecting_) { selecting_ = true; sel_anchor_ = cursor_pos_; }
                move_cursor(-1);
                if (!shift) selecting_ = false;
                kd->consumed = true;
                break;
            case 39: // Right
                if (shift && !selecting_) { selecting_ = true; sel_anchor_ = cursor_pos_; }
                move_cursor(1);
                if (!shift) selecting_ = false;
                kd->consumed = true;
                break;
            case 36: // Home
                if (shift && !selecting_) { selecting_ = true; sel_anchor_ = cursor_pos_; }
                cursor_pos_ = 0;
                if (!shift) selecting_ = false;
                ensure_cursor_visible();
                cursor_blink_ = 0.0f; cursor_visible_ = true;
                kd->consumed = true;
                if (request_repaint_) request_repaint_();
                break;
            case 35: // End
                if (shift && !selecting_) { selecting_ = true; sel_anchor_ = cursor_pos_; }
                cursor_pos_ = text.size();
                if (!shift) selecting_ = false;
                ensure_cursor_visible();
                cursor_blink_ = 0.0f; cursor_visible_ = true;
                kd->consumed = true;
                if (request_repaint_) request_repaint_();
                break;
            case 65: // Ctrl+A
                if (kd->ctrl) {
                    sel_anchor_ = 0;
                    cursor_pos_ = text.size();
                    selecting_ = true;
                    kd->consumed = true;
                    if (request_repaint_) request_repaint_();
                }
                break;
            case 67: // Ctrl+C
                if (kd->ctrl && has_selection()) {
                    clipboard::copy(selected_text());
                    kd->consumed = true;
                }
                break;
            case 86: // Ctrl+V
                if (kd->ctrl) {
                    std::string t = clipboard::paste();
                    if (!t.empty()) { insert_at_cursor(t); kd->consumed = true; }
                }
                break;
            case 88: // Ctrl+X
                if (kd->ctrl && has_selection()) {
                    clipboard::copy(selected_text());
                    delete_selection();
                    kd->consumed = true;
                }
                break;
            case 13: // Enter
                kd->consumed = true;
                if (on_submit) on_submit(text);
                break;
            default:
                break;
            }
        }
    }

    widget::handle_event(type, data);
}

void text_field::paint(std::shared_ptr<renderer> renderer) {
    cached_renderer_ = renderer;

    color border = focused_ ? theme_manager::get(theme_manager::INPUT_FOCUS_BORDER)
                            : theme_manager::get(theme_manager::INPUT_BORDER);

    renderer->draw_rectangle({0, 0, width, height}, theme_manager::get(theme_manager::INPUT_BG));
    renderer->draw_rectangle_outline({0, 0, width, height}, border, 1.5f);

    float text_area_x = padding_h_;
    float text_area_w = width - padding_h_ * 2.0f;
    float text_y = (height - font_size * 1.2f) * 0.5f;

    renderer->push_clip({text_area_x, 0, text_area_w, height});

    if (text.empty() && !focused_) {
        renderer->draw_text(placeholder, {text_area_x, text_y},
            theme_manager::get(theme_manager::INPUT_PLACEHOLDER), font_size, theme_manager::get_str(theme_manager::INPUT_FONT), false);
    } else {
        if (has_selection()) {
            size_t start = std::min(sel_anchor_, cursor_pos_);
            size_t end   = std::max(sel_anchor_, cursor_pos_);
            std::string before = text.substr(0, start);
            std::string sel    = text.substr(start, end - start);
            float sel_x = text_area_x - scroll_x_;
            if (!before.empty()) sel_x += renderer->measure_text_width(before, font_size, theme_manager::get_str(theme_manager::INPUT_FONT));
            float sel_w = renderer->measure_text_width(sel, font_size, theme_manager::get_str(theme_manager::INPUT_FONT));
            color sel_c = {0.20f, 0.40f, 0.80f, 0.35f};
            float sel_h = font_size * 1.4f;
            float sel_y = (height - sel_h) * 0.5f;
            renderer->draw_rectangle({sel_x, sel_y, std::max(sel_w, 1.0f), sel_h}, sel_c);
        }

        renderer->draw_text(text, {text_area_x - scroll_x_, text_y},
            theme_manager::get(theme_manager::INPUT_TEXT), font_size, theme_manager::get_str(theme_manager::INPUT_FONT), false);

        if (focused_ && cursor_visible_) {
            std::string before = text.substr(0, cursor_pos_);
            float cursor_x = text_area_x - scroll_x_;
            if (!before.empty() && renderer) {
                cursor_x += renderer->measure_text_width(before, font_size, theme_manager::get_str(theme_manager::INPUT_FONT));
            }
            float cursor_h = font_size * 1.2f;
            float cursor_y = (height - cursor_h) * 0.5f;
            renderer->draw_rectangle(
                {cursor_x, cursor_y, 1.5f, cursor_h},
                theme_manager::get(theme_manager::INPUT_CURSOR));
        }
    }

    renderer->pop_clip();
}

size text_field::layout_preferred_size() const {
    return {width, font_size + padding_v_ * 2.0f + 8.0f};
}

void text_field::insert_at_cursor(const std::string& s) {
    if (cursor_pos_ > text.size()) cursor_pos_ = text.size();
    if (has_selection()) delete_selection();
    text.insert(cursor_pos_, s);
    cursor_pos_ += s.size();
    sel_anchor_ = cursor_pos_;
    selecting_ = false;
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    ensure_cursor_visible();
    if (on_changed) on_changed(text);
    if (request_repaint_) request_repaint_();
}

void text_field::delete_before_cursor() {
    if (cursor_pos_ == 0) return;
    // 回退到完整 UTF-8 字符起点（跳过连续字节后含前导字节），一次性删除整个字符
    size_t start = cursor_pos_;
    while (start > 0 &&
           (static_cast<unsigned char>(text[start - 1]) & 0xC0) == 0x80)
        --start;
    if (start > 0) --start;
    size_t del = cursor_pos_ - start;
    text.erase(start, del);
    cursor_pos_ = start;
    sel_anchor_ = cursor_pos_;
    selecting_ = false;
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    if (on_changed) on_changed(text);
    if (request_repaint_) request_repaint_();
}

void text_field::delete_after_cursor() {
    if (cursor_pos_ >= text.size()) return;
    size_t del = 1;
    unsigned char c = static_cast<unsigned char>(text[cursor_pos_]);
    if ((c & 0x80) != 0) {
        while (cursor_pos_ + del < text.size() && del < 4 &&
               (static_cast<unsigned char>(text[cursor_pos_ + del]) & 0xC0) == 0x80)
            ++del;
    }
    text.erase(cursor_pos_, del);
    if (on_changed) on_changed(text);
    if (request_repaint_) request_repaint_();
}

void text_field::move_cursor(int delta) {
    if (delta < 0) {
        if (cursor_pos_ == 0) return;
        size_t target = cursor_pos_ - 1;
        while (target > 0 &&
               (static_cast<unsigned char>(text[target]) & 0xC0) == 0x80)
            --target;
        cursor_pos_ = target;
    } else {
        if (cursor_pos_ >= text.size()) return;
        size_t step = 1;
        unsigned char c = static_cast<unsigned char>(text[cursor_pos_]);
        if ((c & 0x80) != 0) {
            while (cursor_pos_ + step < text.size() &&
                   (static_cast<unsigned char>(text[cursor_pos_ + step]) & 0xC0) == 0x80)
                ++step;
        }
        cursor_pos_ += step;
    }
    ensure_cursor_visible();
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    if (request_repaint_) request_repaint_();
}

size_t text_field::hit_to_cursor(float mx, std::shared_ptr<renderer> r) const {
    float text_x = padding_h_;
    mx = mx - text_x + scroll_x_;
    if (mx <= 0.0f || text.empty()) return 0;

    if (r) {
        for (size_t pos = 0; pos <= text.size(); ) {
            std::string prefix = text.substr(0, pos);
            float w = r->measure_text_width(prefix, font_size, theme_manager::get_str(theme_manager::INPUT_FONT));
            if (mx <= w) return pos;
            if (pos >= text.size()) break;
            unsigned char c = static_cast<unsigned char>(text[pos]);
            size_t char_bytes = 1;
            if ((c & 0xF0) == 0xF0) char_bytes = 4;
            else if ((c & 0xE0) == 0xE0) char_bytes = 3;
            else if ((c & 0xC0) == 0xC0) char_bytes = 2;
            pos += char_bytes;
        }
        return text.size();
    }

    float char_w = font_size * 0.60f;
    size_t i = static_cast<size_t>(mx / char_w);
    return std::min(i, text.size());
}

void text_field::ensure_cursor_visible() {
    float cursor_px = 0.0f;
    if (cached_renderer_ && cursor_pos_ > 0) {
        cursor_px = cached_renderer_->measure_text_width(
            text.substr(0, cursor_pos_), font_size, theme_manager::get_str(theme_manager::INPUT_FONT));
    } else {
        float char_w = font_size * 0.60f;
        cursor_px = static_cast<float>(cursor_pos_) * char_w;
    }
    float avail_w = width - padding_h_ * 2.0f - scroll_margin;
    if (avail_w < 10.0f) avail_w = 10.0f;
    if (cursor_px < scroll_x_) {
        scroll_x_ = cursor_px - scroll_margin;
    } else if (cursor_px > scroll_x_ + avail_w) {
        scroll_x_ = cursor_px - avail_w;
    }
    if (scroll_x_ < 0) scroll_x_ = 0;
}

void text_field::open_context_menu(float mx, float my) {
    auto menu = std::make_unique<context_menu>();
    if (has_selection()) {
        menu->add_item(i18n_manager::get().tr("context.copy"), [this]() { clipboard::copy(selected_text()); });
        menu->add_item(i18n_manager::get().tr("context.cut"), [this]() {
            clipboard::copy(selected_text());
            delete_selection();
        });
    }
    menu->add_item(i18n_manager::get().tr("context.paste"), [this]() {
        std::string t = clipboard::paste();
        if (!t.empty()) insert_at_cursor(t);
    });
    menu->add_item(i18n_manager::get().tr("context.select_all"), [this]() {
        sel_anchor_ = 0;
        cursor_pos_ = text.size();
        selecting_ = true;
        if (request_repaint_) request_repaint_();
    });
    point sp = to_screen(mx, my);
    request_context_menu(sp.x, sp.y, std::move(menu));
}

}

