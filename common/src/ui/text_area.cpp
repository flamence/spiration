/**
 * @file text_area.cpp
 * @brief 多行文本编辑器实现。
 * @author clk
 */

#include <ui/text_area.h>
#include <window/event.h>
#include <algorithm>

namespace spiration {

bool text_area::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void text_area::focus() {
    focused_ = true;
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    if (request_repaint_) request_repaint_();
}

void text_area::blur() {
    focused_ = false;
    if (request_repaint_) request_repaint_();
}

void text_area::rebuild_lines() {
    lines_.clear();
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            lines_.push_back({start, i - start});
            start = i + 1;
        }
    }
    lines_.push_back({start, text.size() - start});
}

size_t text_area::line_end(size_t line_idx) const {
    if (line_idx >= lines_.size()) return 0;
    return lines_[line_idx].start + lines_[line_idx].length;
}

void text_area::clamp_cursor() {
    if (lines_.empty()) { cursor_line_ = 0; cursor_col_ = 0; return; }
    if (cursor_line_ >= lines_.size()) cursor_line_ = lines_.size() - 1;
    if (cursor_col_ > lines_[cursor_line_].length) cursor_col_ = lines_[cursor_line_].length;
}

void text_area::ensure_cursor_visible() {
    float cy = static_cast<float>(cursor_line_) * line_height;
    if (cy < scroll_y_) scroll_y_ = cy;
    else if (cy + line_height > scroll_y_ + height) scroll_y_ = cy + line_height - height;
    if (scroll_y_ < 0.0f) scroll_y_ = 0.0f;
}

void text_area::tick(float dt_ms) {
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

void text_area::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        if (md->action == mouse_action::down) {
            if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
                md->consumed = true;
                focus();
                float cy = my + scroll_y_;
                cursor_line_ = static_cast<size_t>(std::max(0.0f, cy / line_height));
                clamp_cursor();
                cursor_col_ = static_cast<size_t>(std::max(0.0f, (mx - padding_h_) / (font_size * 0.6f)));
                clamp_cursor();
                cursor_blink_ = 0.0f; cursor_visible_ = true;
                if (request_repaint_) request_repaint_();
            } else { blur(); }
        }
        if (md->action == mouse_action::wheel) {
            float step = (md->wheel_delta > 0) ? -line_height * 3.0f : line_height * 3.0f;
            scroll_y_ = std::max(0.0f, scroll_y_ + step);
            md->consumed = true;
            if (request_repaint_) request_repaint_();
        }
    } else if (type == event_type::keyboard && focused_) {
        auto* kd = static_cast<key_event_data*>(data);
        rebuild_lines();

        if (kd->codepoint >= 32 && kd->codepoint != 127) {
            std::string s;
            if (kd->codepoint <= 0x7F) s += static_cast<char>(kd->codepoint);
            else if (kd->codepoint <= 0x7FF) {
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
            case 8:  delete_before_cursor(); kd->consumed = true; break;
            case 46: delete_after_cursor(); kd->consumed = true; break;
            case 37: move_cursor_h(-1); kd->consumed = true; break;
            case 39: move_cursor_h(1); kd->consumed = true; break;
            case 38: move_cursor_v(-1); kd->consumed = true; break;
            case 40: move_cursor_v(1); kd->consumed = true; break;
            case 13: new_line(); kd->consumed = true; break;
            case 36: cursor_col_ = 0; cursor_blink_ = 0.0f; cursor_visible_ = true; kd->consumed = true; if (request_repaint_) request_repaint_(); break;
            case 35: cursor_col_ = lines_.empty() ? 0 : lines_[cursor_line_].length; cursor_blink_ = 0.0f; cursor_visible_ = true; kd->consumed = true; if (request_repaint_) request_repaint_(); break;
            default: break;
            }
        }
    }
    widget::handle_event(type, data);
}

void text_area::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({x, y, width, height}, theme::get(theme::INPUT_BG));
    renderer->draw_rectangle_outline({x, y, width, height},
        focused_ ? theme::get(theme::INPUT_FOCUS_BORDER) : theme::get(theme::INPUT_BORDER), 1.5f);

    renderer->push_clip({x + padding_h_, y, width - padding_h_ * 2.0f, height});

    if (text.empty() && !focused_) {
        renderer->draw_text("", {x + padding_h_, y - scroll_y_}, theme::get(theme::INPUT_PLACEHOLDER), font_size);
    }

    float cy = y - scroll_y_;
    for (size_t li = 0; li < lines_.size(); ++li) {
        if (cy + line_height < y || cy > y + height) { cy += line_height; continue; }
        std::string line = text.substr(lines_[li].start, lines_[li].length);
        renderer->draw_text(line, {x + padding_h_, cy}, theme::get(theme::INPUT_TEXT), font_size);
        cy += line_height;
    }

    renderer->pop_clip();

    if (focused_ && cursor_visible_ && cursor_line_ < lines_.size()) {
        float cx = x + padding_h_ + cursor_col_ * font_size * 0.6f;
        float cy = y + cursor_line_ * line_height - scroll_y_;
        renderer->draw_rectangle({cx, cy, 1.5f, line_height}, theme::get(theme::INPUT_CURSOR));
    }
}

size text_area::layout_preferred_size() const {
    return {width, height};
}

void text_area::insert_at_cursor(const std::string& s) {
    if (lines_.empty()) lines_.push_back({0, 0});
    if (cursor_line_ >= lines_.size()) cursor_line_ = lines_.size() - 1;
    size_t pos = lines_[cursor_line_].start + cursor_col_;
    text.insert(pos, s);
    cursor_col_ += s.size();
    rebuild_lines();
    if (on_changed) on_changed(text);
    ensure_cursor_visible();
    if (request_repaint_) request_repaint_();
}

void text_area::delete_before_cursor() {
    if (lines_.empty()) return;
    if (cursor_line_ >= lines_.size()) cursor_line_ = lines_.size() - 1;
    if (cursor_col_ == 0) {
        if (cursor_line_ == 0) return;
        size_t prev_end = line_end(cursor_line_ - 1);
        size_t cur_start = lines_[cursor_line_].start;
        text.erase(prev_end, 1);
        cursor_line_--;
        cursor_col_ = lines_[cursor_line_].length;
    } else {
        size_t pos = lines_[cursor_line_].start + cursor_col_ - 1;
        text.erase(pos, 1);
        cursor_col_--;
    }
    rebuild_lines();
    if (on_changed) on_changed(text);
    ensure_cursor_visible();
    if (request_repaint_) request_repaint_();
}

void text_area::delete_after_cursor() {
    if (lines_.empty()) return;
    if (cursor_line_ >= lines_.size()) return;
    size_t pos = lines_[cursor_line_].start + cursor_col_;
    if (pos >= text.size()) return;
    text.erase(pos, 1);
    rebuild_lines();
    if (on_changed) on_changed(text);
    if (request_repaint_) request_repaint_();
}

void text_area::new_line() {
    if (lines_.empty()) lines_.push_back({0, 0});
    if (cursor_line_ >= lines_.size()) cursor_line_ = lines_.size() - 1;
    size_t pos = lines_[cursor_line_].start + cursor_col_;
    text.insert(pos, "\n");
    cursor_line_++; cursor_col_ = 0;
    rebuild_lines();
    if (on_changed) on_changed(text);
    ensure_cursor_visible();
    if (request_repaint_) request_repaint_();
}

void text_area::move_cursor_h(int delta) {
    if (delta < 0 && cursor_col_ > 0) cursor_col_--;
    else if (delta > 0 && cursor_line_ < lines_.size() && cursor_col_ < lines_[cursor_line_].length) cursor_col_++;
    cursor_blink_ = 0.0f; cursor_visible_ = true;
    if (request_repaint_) request_repaint_();
}

void text_area::move_cursor_v(int delta) {
    if (lines_.empty()) return;
    if (delta < 0 && cursor_line_ > 0) cursor_line_--;
    else if (delta > 0 && cursor_line_ + 1 < lines_.size()) cursor_line_++;
    clamp_cursor();
    cursor_blink_ = 0.0f; cursor_visible_ = true;
    if (request_repaint_) request_repaint_();
}

}
