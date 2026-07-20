/**
 * @file text_field.cpp
 * @brief 单行文本输入框实现，支持键盘输入和光标操作�?
 * @author clk
 */

#include <ui/text_field.h>
#include <window/event.h>

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

void text_field::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        if (md->action == mouse_action::down) {
            if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
                md->consumed = true;
                focus();
                cursor_pos_ = hit_to_cursor(mx);
                cursor_blink_ = 0.0f;
                cursor_visible_ = true;
                if (request_repaint_) request_repaint_();
            } else {
                blur();
            }
        }
    } else if (type == event_type::keyboard && focused_) {
        auto* kd = static_cast<key_event_data*>(data);

        if (kd->codepoint >= 32 && kd->codepoint != 127) {
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
            case 8:
                delete_before_cursor();
                kd->consumed = true;
                break;
            case 46:
                delete_after_cursor();
                kd->consumed = true;
                break;
            case 37:
                move_cursor(-1);
                kd->consumed = true;
                break;
            case 39:
                move_cursor(1);
                kd->consumed = true;
                break;
            case 36:
                cursor_pos_ = 0;
                cursor_blink_ = 0.0f;
                cursor_visible_ = true;
                kd->consumed = true;
                if (request_repaint_) request_repaint_();
                break;
            case 35:
                cursor_pos_ = text.size();
                cursor_blink_ = 0.0f;
                cursor_visible_ = true;
                kd->consumed = true;
                if (request_repaint_) request_repaint_();
                break;
            case 13:
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
    color border = focused_ ? theme_manager::get(theme_manager::INPUT_FOCUS_BORDER)
                            : theme_manager::get(theme_manager::INPUT_BORDER);

    renderer->draw_rectangle({x, y, width, height}, theme_manager::get(theme_manager::INPUT_BG));
    renderer->draw_rectangle_outline({x, y, width, height}, border, 1.5f);

    float text_area_x = x + padding_h_;
    float text_area_y = y;
    float text_area_w = width - padding_h_ * 2.0f;
    float text_area_h = height;

    if (text.empty() && !focused_) {
        renderer->draw_text_aligned(
            placeholder,
            {text_area_x, text_area_y, text_area_w, text_area_h},
            theme_manager::get(theme_manager::INPUT_PLACEHOLDER),
            text_alignment::left,
            vertical_alignment::center,
            font_size);
    } else {
        renderer->draw_text_aligned(
            text,
            {text_area_x, text_area_y, text_area_w, text_area_h},
            theme_manager::get(theme_manager::INPUT_TEXT),
            text_alignment::left,
            vertical_alignment::center,
            font_size);

        if (focused_ && cursor_visible_) {
            std::string before = text.substr(0, cursor_pos_);
            float cursor_x = text_area_x;
            if (!before.empty() && renderer) {
                cursor_x += renderer->measure_text_width(before, font_size);
            }
            float cursor_h = font_size * 1.2f;
            float cursor_y = y + (height - cursor_h) * 0.5f;
            renderer->draw_rectangle(
                {cursor_x, cursor_y, 1.5f, cursor_h},
                theme_manager::get(theme_manager::INPUT_CURSOR));
        }
    }
}

size text_field::layout_preferred_size() const {
    return {width, font_size + padding_v_ * 2.0f + 8.0f};
}

void text_field::insert_at_cursor(const std::string& s) {
    text.insert(cursor_pos_, s);
    cursor_pos_ += s.size();
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    if (on_changed) on_changed(text);
    if (request_repaint_) request_repaint_();
}

void text_field::delete_before_cursor() {
    if (cursor_pos_ == 0) return;
    size_t del = 1;
    if (cursor_pos_ > 0) {
        unsigned char c = static_cast<unsigned char>(text[cursor_pos_ - 1]);
        if ((c & 0x80) != 0) {
            while (del < cursor_pos_ && del < 4 &&
                   (static_cast<unsigned char>(text[cursor_pos_ - 1 - del]) & 0xC0) == 0x80)
                ++del;
        }
    }
    text.erase(cursor_pos_ - del, del);
    cursor_pos_ -= del;
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
        size_t step = 1;
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
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    if (request_repaint_) request_repaint_();
}

size_t text_field::hit_to_cursor(float mx) const {
    float text_x = padding_h_;
    mx -= text_x;
    if (mx <= 0.0f || text.empty()) return 0;

    float char_w = font_size * 0.60f;
    for (size_t i = 1; i <= text.size(); ++i) {
        if (mx <= static_cast<float>(i) * char_w) return i;
    }
    return text.size();
}

}
