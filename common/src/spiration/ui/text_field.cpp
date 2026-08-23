/**
 * @file text_field.cpp
 * @brief 单行文本输入框实现。
 * @author clk
 */

#include <spiration/ui/widget.h>
#include <string>
#include <spiration/ui/text_field.h>
#include <spiration/ui/context_menu.h>
#include <spiration/ui/focus_manager.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/window/event.h>
#include <spiration/window/ime.h>
#include <spiration/utils/clipboard.h>
#include <algorithm>

namespace spiration {

namespace {

/// @brief 文本行数（含末尾空行，至少 1）。
size_t tf_line_count(const std::string& t) {
    if (t.empty()) return 1;
    size_t n = 1;
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i] == '\n') ++n;
    return n;
}

/// @brief 第 line 行的首字节偏移。
size_t tf_line_start(const std::string& t, size_t line) {
    size_t start = 0;
    size_t cur = 0;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == '\n') {
            if (cur == line) return start;
            ++cur;
            start = i + 1;
        }
    }
    return (cur == line) ? start : t.size();
}

} // namespace

bool text_field::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void text_field::focus() {
    focus_manager::instance().request_focus(this);
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    if (request_repaint_) request_repaint_();
}

void text_field::blur() {
    if (focus_manager::instance().focused() == this)
        focus_manager::instance().clear_focus();
    if (request_repaint_) request_repaint_();
}

void text_field::on_focus() {
    focused_ = true;
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    if (request_repaint_) request_repaint_();
}

void text_field::on_blur() {
    focused_ = false;
    selecting_ = false;
    cursor_visible_ = false;
    composition_text_.clear();
    composition_cursor_ = -1;
    if (request_repaint_) request_repaint_();
}

void text_field::blur_current() {
    focus_manager::instance().clear_focus();
}

void text_field::select_all() {
    sel_anchor_ = 0;
    cursor_pos_ = text.size();
    selecting_ = true;
    cursor_blink_ = 0.0f;
    cursor_visible_ = true;
    ensure_cursor_visible();
    if (request_repaint_) request_repaint_();
}

text_field* text_field::current_focused() {
    return dynamic_cast<text_field*>(focus_manager::instance().focused());
}

void text_field::tick(float dt_ms) {
    if (focused_) {
        cursor_blink_ += dt_ms;
        if (dt_ms > 500.0f) cursor_blink_ = cursor_blink_interval_;
        if (cursor_blink_ >= cursor_blink_interval_) {
            cursor_blink_ -= cursor_blink_interval_;
            cursor_visible_ = !cursor_visible_;
            if (request_repaint_) request_repaint_();
        }
    }
    widget::tick(dt_ms);
}

void text_field::layout() {
    on_layout_begin();
    if (widget_style.height > 0) {
        height = static_cast<float>(widget_style.height);
    } else {
        label::layout();
    }
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
    if (type == event_type::ime_composition) {
        if (!focused_) return;
        auto* ime = static_cast<ime_composition_data*>(data);
        set_composition(ime->text, ime->cursor);
        return;
    }

    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        if (md->action == mouse_action::down && md->button == mouse_button::right) {
            if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
                focus();
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
                sel_anchor_ = hit_to_cursor(mx, my, cached_renderer_);
                cursor_pos_ = sel_anchor_;
                ensure_cursor_visible();
                cursor_blink_ = 0.0f;
                cursor_visible_ = true;
                if (request_repaint_) request_repaint_();
            }
        } else if (md->action == mouse_action::move && mouse_down_) {
            cursor_pos_ = hit_to_cursor(mx, my, cached_renderer_);
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
                    if (!multiline && !t.empty()) {
                        // 单行：多行粘贴内容合并为空格分隔的一行。
                        for (size_t i = 0; i < t.size();) {
                            if (t[i] == '\r' || t[i] == '\n') {
                                size_t j = i;
                                while (j < t.size() && (t[j] == '\r' || t[j] == '\n')) ++j;
                                t.replace(i, j - i, " ");
                                i += 1;
                            } else {
                                ++i;
                            }
                        }
                    }
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
                if (multiline && shift) {
                    insert_at_cursor("\n");
                } else if (on_submit) {
                    on_submit(text);
                }
                break;
            default:
                break;
            }
        }
    }

    if (focused_) report_ime_caret();

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
    const float line_h = font_size * 1.4f;
    const std::string fam = theme_manager::get_str(theme_manager::INPUT_FONT);

    renderer->push_clip({text_area_x, 0, text_area_w, height});

    if (text.empty() && !focused_) {
        renderer->draw_text(placeholder, {text_area_x, content_top()},
            theme_manager::get(theme_manager::INPUT_PLACEHOLDER), font_size, fam, false);
    } else {
        const size_t n = tf_line_count(text);
        const bool sel = has_selection();
        const size_t sela = sel ? std::min(sel_anchor_, cursor_pos_) : 0;
        const size_t selb = sel ? std::max(sel_anchor_, cursor_pos_) : 0;
        const float top = content_top();

        size_t cur_line = 0;
        size_t cur_line_start = 0;
        size_t li_start = 0;
        for (size_t li = 0; li < n; ++li) {
            size_t ls = li_start;
            size_t le = ls;
            while (le < text.size() && text[le] != '\n') ++le;
            std::string lt = text.substr(ls, le - ls);
            const float ly = top + static_cast<float>(li) * line_h - scroll_y_;

            // 组合输入：光标所在行拆为 前缀 + 组合文本(下划线) + 后缀。
            const bool comp_line = has_composition() && cursor_pos_ >= ls && cursor_pos_ <= le;
            if (comp_line) {
                const size_t rel = cursor_pos_ - ls;
                const std::string prefix = lt.substr(0, rel);
                const std::string suffix = lt.substr(rel);
                float cx = text_area_x - scroll_x_;
                if (!prefix.empty()) {
                    renderer->draw_text(prefix, {cx, ly},
                                        theme_manager::get(theme_manager::INPUT_TEXT), font_size, fam, false);
                    cx += renderer->measure_text_width(prefix, font_size, fam);
                }
                if (!composition_text_.empty()) {
                    renderer->draw_text(composition_text_, {cx, ly},
                                        theme_manager::get(theme_manager::INPUT_TEXT), font_size, fam, false);
                    const float cw = renderer->measure_text_width(composition_text_, font_size, fam);
                    renderer->draw_rectangle({cx, ly + font_size + 3.0f, cw, 1.5f},
                                             theme_manager::get(theme_manager::INPUT_CURSOR));
                    cx += cw;
                }
                if (!suffix.empty()) {
                    renderer->draw_text(suffix, {cx, ly},
                                        theme_manager::get(theme_manager::INPUT_TEXT), font_size, fam, false);
                }
            } else {
                renderer->draw_text(lt, {text_area_x - scroll_x_, ly},
                                    theme_manager::get(theme_manager::INPUT_TEXT), font_size, fam, false);
            }

            if (cursor_pos_ >= ls && cursor_pos_ <= le) {
                cur_line = li;
                cur_line_start = ls;
            }

            if (sel && le > sela && ls < selb) {
                const size_t ia = (sela > ls) ? sela : ls;
                const size_t ib = (selb < le) ? selb : le;
                float xa = text_area_x - scroll_x_;
                float xb = text_area_x - scroll_x_;
                if (ia > ls)
                    xa += renderer->measure_text_width(lt.substr(0, ia - ls), font_size, fam);
                if (ib >= ls)
                    xb += renderer->measure_text_width(lt.substr(0, ib - ls), font_size, fam);
                color sel_c = {0.20f, 0.40f, 0.80f, 0.35f};
                renderer->draw_rectangle({xa, ly, std::max(xb - xa, 1.0f), line_h}, sel_c);
            }

            li_start = le + 1;
        }

        if (focused_ && cursor_visible_) {
            std::string before = text.substr(cur_line_start, cursor_pos_ - cur_line_start);
            float cursor_x = text_area_x - scroll_x_;
            if (!before.empty())
                cursor_x += renderer->measure_text_width(before, font_size, fam);
            // 组合输入：光标停在组合文本内光标处。
            if (has_composition()) {
                const size_t comp_bytes = utf16_offset_to_utf8_bytes(composition_text_, composition_cursor_);
                cursor_x += renderer->measure_text_width(
                    composition_text_.substr(0, comp_bytes), font_size, fam);
            }
            float cursor_y = top + static_cast<float>(cur_line) * line_h - scroll_y_;
            renderer->draw_rectangle({cursor_x, cursor_y, 1.5f, line_h},
                                     theme_manager::get(theme_manager::INPUT_CURSOR));
        }
    }

    renderer->pop_clip();
}

float text_field::content_top() const {
    if (!multiline) return (height - font_size * 1.2f) * 0.5f;
    const float line_h = font_size * 1.4f;
    const float content_h = static_cast<float>(tf_line_count(text)) * line_h;
    float top = (height - content_h) * 0.5f;
    if (top < padding_v_) top = padding_v_;
    return top;
}

size text_field::layout_preferred_size() const {
    if (multiline) {
        const float line_h = font_size * 1.4f;
        const float h = static_cast<float>(tf_line_count(text)) * line_h + padding_v_ * 2.0f;
        return {width, std::max(h, font_size + padding_v_ * 2.0f + 8.0f)};
    }
    return {width, font_size + padding_v_ * 2.0f + 8.0f};
}

void text_field::clear() {
    text.clear();
    cursor_pos_ = 0;
    sel_anchor_ = 0;
    selecting_ = false;
    scroll_x_ = 0.0f;
    scroll_y_ = 0.0f;
    if (on_changed) on_changed(text);
    if (request_repaint_) request_repaint_();
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
    ensure_cursor_visible();
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
    ensure_cursor_visible();
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

size_t text_field::hit_to_cursor(float mx, float my, std::shared_ptr<renderer> r) const {
    if (multiline) {
        const float line_h = font_size * 1.4f;
        const float top = content_top();
        const size_t n = tf_line_count(text);
        size_t line = n - 1;
        for (size_t i = 0; i < n; ++i) {
            const float y0 = top + static_cast<float>(i) * line_h - scroll_y_;
            if (my < y0 + line_h) { line = i; break; }
        }
        const size_t ls = tf_line_start(text, line);
        size_t le = ls;
        while (le < text.size() && text[le] != '\n') ++le;
        const std::string lt = text.substr(ls, le - ls);

        const float text_x = padding_h_;
        float rel = mx - text_x + scroll_x_;
        if (rel <= 0.0f || lt.empty()) return ls;

        if (r) {
            for (size_t pos = 0; pos <= lt.size(); ) {
                std::string prefix = lt.substr(0, pos);
                float w = r->measure_text_width(prefix, font_size,
                                                theme_manager::get_str(theme_manager::INPUT_FONT));
                if (rel <= w) return ls + pos;
                if (pos >= lt.size()) break;
                unsigned char c = static_cast<unsigned char>(lt[pos]);
                size_t char_bytes = 1;
                if ((c & 0xF0) == 0xF0) char_bytes = 4;
                else if ((c & 0xE0) == 0xE0) char_bytes = 3;
                else if ((c & 0xC0) == 0xC0) char_bytes = 2;
                pos += char_bytes;
            }
            return ls + lt.size();
        }

        float char_w = font_size * 0.60f;
        size_t i = static_cast<size_t>(rel / char_w);
        if (i > lt.size()) i = lt.size();
        return ls + i;
    }

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
    // 光标所在行与行前缀。
    size_t line = 0;
    size_t line_start = 0;
    for (size_t i = 0; i < cursor_pos_ && i < text.size(); ++i) {
        if (text[i] == '\n') {
            ++line;
            line_start = i + 1;
        }
    }
    std::string line_prefix = text.substr(line_start, cursor_pos_ - line_start);
    float cursor_px = 0.0f;
    if (cached_renderer_) {
        cursor_px = cached_renderer_->measure_text_width(
            line_prefix, font_size, theme_manager::get_str(theme_manager::INPUT_FONT));
    } else {
        float char_w = font_size * 0.60f;
        cursor_px = static_cast<float>(line_prefix.size()) * char_w;
    }
    float avail_w = width - padding_h_ * 2.0f - scroll_margin;
    if (avail_w < 10.0f) avail_w = 10.0f;
    if (cursor_px < scroll_x_) {
        scroll_x_ = cursor_px - scroll_margin;
    } else if (cursor_px > scroll_x_ + avail_w) {
        scroll_x_ = cursor_px - avail_w;
    }
    // 横向滚动上限取当前行宽度（多行时各行独立）。
    float line_w = cursor_px;
    {
        size_t le = line_start;
        while (le < text.size() && text[le] != '\n') ++le;
        std::string lt = text.substr(line_start, le - line_start);
        if (cached_renderer_) {
            line_w = cached_renderer_->measure_text_width(
                lt, font_size, theme_manager::get_str(theme_manager::INPUT_FONT));
        } else {
            line_w = static_cast<float>(lt.size()) * font_size * 0.60f;
        }
    }
    float max_scroll = std::max(0.0f, line_w - avail_w);
    if (scroll_x_ > max_scroll) scroll_x_ = max_scroll;
    if (scroll_x_ < 0) scroll_x_ = 0;

    if (!multiline) return;

    const float line_h = font_size * 1.4f;
    const float top = content_top();
    const float cy = top + static_cast<float>(line) * line_h;
    if (cy < padding_v_) {
        scroll_y_ = cy - padding_v_;
    } else if (cy + line_h > height - padding_v_) {
        scroll_y_ = cy + line_h - (height - padding_v_);
    }
    const float content_h = static_cast<float>(tf_line_count(text)) * line_h;
    const float max_scroll_y = std::max(0.0f, top + content_h - height);
    if (scroll_y_ > max_scroll_y) scroll_y_ = max_scroll_y;
    if (scroll_y_ < 0) scroll_y_ = 0;
}

void text_field::report_ime_caret() {
    size_t line = 0;
    size_t line_start = 0;
    for (size_t i = 0; i < cursor_pos_ && i < text.size(); ++i) {
        if (text[i] == '\n') {
            ++line;
            line_start = i + 1;
        }
    }
    std::string line_prefix = text.substr(line_start, cursor_pos_ - line_start);
    float line_w = 0.0f;
    if (!line_prefix.empty()) {
        if (cached_renderer_) {
            line_w = cached_renderer_->measure_text_width(
                line_prefix, font_size, theme_manager::get_str(theme_manager::INPUT_FONT));
        } else {
            line_w = static_cast<float>(line_prefix.size()) * font_size * 0.6f;
        }
    }
    float line_h = font_size * 1.4f;
    float ime_x = text_x() - scroll_x_ + line_w;
    float ime_y = content_top() + static_cast<float>(line) * line_h - scroll_y_;
    point sp = to_screen(ime_x, ime_y);
    ime::set_caret_rect(sp.x, sp.y);
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

