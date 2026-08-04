/**
 * @file text_area.cpp
 * @brief 多行文本编辑器实现。
 * @author clk
 */

#include <ui/text_area.h>
#include <ui/context_menu.h>
#include <extension/builtin/i18n/i18n.h>
#include <utils/clipboard.h>
#include <window/event.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>

namespace spiration {

namespace {

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool is_word_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string codepoint_to_utf8(unsigned int cp) {
    std::string result;
    if (cp < 0x80) {
        result += static_cast<char>(cp);
    } else if (cp < 0x800) {
        result += static_cast<char>(0xC0 | (cp >> 6));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        result += static_cast<char>(0xE0 | (cp >> 12));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
        result += static_cast<char>(0xF0 | (cp >> 18));
        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return result;
}

void normalize_selection(size_t cl, size_t cc, size_t al, size_t ac,
                         size_t& sl, size_t& sc, size_t& el, size_t& ec) {
    if (cl < al || (cl == al && cc < ac)) {
        sl = cl; sc = cc; el = al; ec = ac;
    } else {
        sl = al; sc = ac; el = cl; ec = cc;
    }
}

} // namespace

bool text_area::hit_test(float x, float y) const {
    return x >= 0.0f && x <= width && y >= 0.0f && y <= height;
}

void text_area::focus() {
    focused_ = true;
    cursor_visible_ = true;
    cursor_timer_ = 0.0f;
    if (request_repaint_) request_repaint_();
}

void text_area::blur() {
    focused_ = false;
    if (request_repaint_) request_repaint_();
}

void text_area::rebuild_line_starts() {
    line_starts_.clear();
    line_starts_.push_back(0);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') line_starts_.push_back(i + 1);
    }
    line_starts_.push_back(text.size());
    line_starts_dirty_ = false;
    max_width_dirty_ = true;
}

size_t text_area::line_count() const {
    if (line_starts_dirty_ || line_starts_.empty())
        const_cast<text_area*>(this)->rebuild_line_starts();
    if (line_starts_.size() <= 1) return 0;
    return line_starts_.size() - 1;
}

std::string_view text_area::line_text(size_t n) const {
    if (line_starts_dirty_ || line_starts_.empty())
        const_cast<text_area*>(this)->rebuild_line_starts();
    if (line_starts_.size() <= 1 || n >= line_starts_.size() - 1) return {};
    size_t start = line_starts_[n];
    size_t end = line_starts_[n + 1];
    if (end > start) {
        if (text[end - 1] == '\n') --end;
        if (end > start && text[end - 1] == '\r') --end;
    }
    return std::string_view(text.data() + start, end - start);
}

void text_area::scroll_to_y(float y) {
    float max_scroll = std::max(0.0f,
        static_cast<float>(line_count()) * line_height + text_pad_top - height);
    scroll_y_ = std::max(0.0f, std::min(y, max_scroll));
    if (request_repaint_) request_repaint_();
}

void text_area::scroll_to_x(float x) {
    scroll_x_ = std::max(0.0f, x);
    if (!max_width_dirty_) scroll_x_ = std::min(scroll_x_, scroll_max_x());
    if (request_repaint_) request_repaint_();
}

float text_area::scroll_max_x() const {
    return std::max(0.0f, max_line_width_ + text_pad_right - (width - text_x()));
}

float text_area::compute_max_line_width(std::shared_ptr<renderer> r) {
    max_line_width_ = 0.0f;
    size_t n = line_count();
    for (size_t i = 0; i < n; ++i) {
        std::string_view sv = line_text(i);
        if (sv.empty()) continue;
        float w = r->measure_text_width(std::string(sv.data(), sv.size()),
                                        font_size, font_family);
        if (w > max_line_width_) max_line_width_ = w;
    }
    max_width_dirty_ = false;
    return max_line_width_;
}

void text_area::reset_view() {
    line_starts_dirty_ = true;
    max_width_dirty_ = true;
    cursor_line_ = 0;
    cursor_col_ = 0;
    selecting_ = false;
    scroll_y_ = 0.0f;
    scroll_x_ = 0.0f;
    cursor_pixel_x_ = -1.0f;
    cursor_visible_ = true;
    cursor_timer_ = 0.0f;
}

bool text_area::has_selection() const {
    if (!selecting_) return false;
    return cursor_line_ != sel_anchor_line_ || cursor_col_ != sel_anchor_col_;
}

std::string text_area::selected_text() const {
    return get_selected_text();
}

void text_area::select_all() { handle_select_all(); }
void text_area::copy()       { handle_copy(); }
void text_area::cut()        { handle_cut(); }
void text_area::paste()      { handle_paste(); }

void text_area::tick(float dt_ms) {
    bool repaint = false;
    repaint |= scrollbar_thumb_bg_.update(dt_ms);
    repaint |= h_scrollbar_thumb_bg_.update(dt_ms);
    if (repaint && request_repaint_) request_repaint_();

    if (focused_) {
        cursor_timer_ += dt_ms;
        if (cursor_timer_ >= CURSOR_BLINK_MS) {
            cursor_visible_ = !cursor_visible_;
            cursor_timer_ = 0.0f;
            if (request_repaint_) request_repaint_();
        }
    }

    widget::tick(dt_ms);
}

void text_area::handle_event(const event_type& type, void* data) {
    if (!data) return;

    if (type == event_type::keyboard) {
        if (!focused_) return;
        auto* key = static_cast<key_event_data*>(data);
        handle_key(*key);
        return;
    }

    if (type == event_type::mouse) {
        auto* mouse = static_cast<mouse_event_data*>(data);

        if (mouse->action == mouse_action::down && mouse->button == mouse_button::right) {
            if (mouse->position.x >= 0.0f && mouse->position.x <= width &&
                mouse->position.y >= 0.0f && mouse->position.y <= height) {
                open_context_menu(mouse->position.x, mouse->position.y);
                mouse->consumed = true;
            }
            return;
        }

        if (mouse->action == mouse_action::down && mouse->button == mouse_button::left) {
            if (mouse->position.x < 0.0f || mouse->position.x > width ||
                mouse->position.y < 0.0f || mouse->position.y > height) {
                return;
            }
            focus();
            if (is_on_scrollbar_thumb(mouse->position.x, mouse->position.y)) {
                scrollbar_dragging_ = true;
                scrollbar_drag_start_y_ = mouse->position.y;
                scrollbar_drag_start_scroll_ = scroll_y_;
                set_mouse_capture(this);
                mouse->consumed = true;
                if (request_repaint_) request_repaint_();
                return;
            }
            if (is_on_h_scrollbar_thumb(mouse->position.x, mouse->position.y)) {
                h_scrollbar_dragging_ = true;
                h_scrollbar_drag_start_x_ = mouse->position.x;
                h_scrollbar_drag_start_scroll_ = scroll_x_;
                set_mouse_capture(this);
                mouse->consumed = true;
                if (request_repaint_) request_repaint_();
                return;
            }
            long long t = now_ms();
            pending_click_.dbl = (t - last_click_ms_ < DBL_CLICK_MS);
            last_click_ms_ = t;

            mouse_down_ = true;
            selecting_ = false;
            pending_is_drag_ = false;
            drag_resolved_ = false;
            press_x_ = mouse->position.x;
            press_y_ = mouse->position.y;
            pending_click_.x = mouse->position.x;
            pending_click_.y = mouse->position.y;
            pending_click_.shift = mouse->shift;
            pending_click_.active = true;
            set_mouse_capture(this);
            mouse->consumed = true;
            if (request_repaint_) request_repaint_();
        } else if (mouse->action == mouse_action::move &&
                   (mouse_down_ || scrollbar_dragging_ || h_scrollbar_dragging_)) {
            if (scrollbar_dragging_) {
                float dy = mouse->position.y - scrollbar_drag_start_y_;
                float ty, th, ms;
                scrollbar_thumb_rect(ty, th, ms);
                if (ms > 0.0f) {
                    float track = std::max(1.0f, height - th);
                    scroll_y_ = scrollbar_drag_start_scroll_ + dy / track * ms;
                    scroll_y_ = std::max(0.0f, std::min(scroll_y_, ms));
                }
                if (request_repaint_) request_repaint_();
            } else if (h_scrollbar_dragging_) {
                float dx = mouse->position.x - h_scrollbar_drag_start_x_;
                float hx, hw, hms;
                h_scrollbar_thumb_rect(hx, hw, hms);
                if (hms > 0.0f) {
                    float track = std::max(1.0f, width - hw);
                    scroll_x_ = h_scrollbar_drag_start_scroll_ + dx / track * hms;
                    scroll_x_ = std::max(0.0f, std::min(scroll_x_, hms));
                }
                if (request_repaint_) request_repaint_();
            } else if (mouse_down_) {
                pending_is_drag_ = true;
                pending_click_.x = mouse->position.x;
                pending_click_.y = mouse->position.y;
                pending_click_.active = true;
                mouse->consumed = true;
                if (request_repaint_) request_repaint_();
            }
        } else if (mouse->action == mouse_action::move) {
            bool over = is_on_scrollbar_thumb(mouse->position.x, mouse->position.y);
            if (over != scrollbar_hovering_) {
                scrollbar_hovering_ = over;
                if (over) scrollbar_thumb_bg_.animate_to({0.55f, 0.55f, 0.55f, 0.8f}, 100.0f);
                else scrollbar_thumb_bg_.animate_to({0.45f, 0.45f, 0.45f, 0.5f}, 150.0f);
                if (request_repaint_) request_repaint_();
            }
            bool over_h = is_on_h_scrollbar_thumb(mouse->position.x, mouse->position.y);
            if (over_h != h_scrollbar_hovering_) {
                h_scrollbar_hovering_ = over_h;
                if (over_h) h_scrollbar_thumb_bg_.animate_to({0.55f, 0.55f, 0.55f, 0.8f}, 100.0f);
                else h_scrollbar_thumb_bg_.animate_to({0.45f, 0.45f, 0.45f, 0.5f}, 150.0f);
                if (request_repaint_) request_repaint_();
            }
        } else if (mouse->action == mouse_action::up && mouse->button == mouse_button::left) {
            set_mouse_capture(nullptr);
            scrollbar_dragging_ = false;
            h_scrollbar_dragging_ = false;
            mouse_down_ = false;
            mouse->consumed = true;
        } else if (mouse->action == mouse_action::wheel) {
            if (mouse->shift) {
                scroll_x_ -= static_cast<float>(mouse->wheel_delta) * 6.0f;
                scroll_x_ = std::max(0.0f, std::min(scroll_x_, scroll_max_x()));
            } else {
                scroll_y_ -= static_cast<float>(mouse->wheel_delta) * 3.0f;
                float max_scroll = std::max(0.0f,
                    static_cast<float>(line_count()) * line_height + text_pad_top - height);
                scroll_y_ = std::max(0.0f, std::min(scroll_y_, max_scroll));
            }
            mouse->consumed = true;
            if (request_repaint_) request_repaint_();
        }
        return;
    }

    widget::handle_event(type, data);
}

void text_area::open_context_menu(float mx, float my) {
    auto menu = std::make_unique<context_menu>();
    if (has_selection()) {
        menu->add_item(i18n_manager::get().tr("context.copy"), [this]() { copy(); });
        menu->add_item(i18n_manager::get().tr("context.cut"), [this]() { cut(); });
    }
    menu->add_item(i18n_manager::get().tr("context.paste"), [this]() { paste(); });
    menu->add_item(i18n_manager::get().tr("context.select_all"), [this]() {
        select_all();
        if (request_repaint_) request_repaint_();
    });
    point sp = to_screen(mx, my);
    request_context_menu(sp.x, sp.y, std::move(menu));
}

void text_area::handle_key(const key_event_data& key) {
    shift_pressed_ = key.shift;

    if (key.key_code != 13) suppress_char_after_enter_ = false;

    bool is_move = (key.key_code >= 37 && key.key_code <= 40) ||
                   key.key_code == 36 || key.key_code == 35;

    if (is_move) {
        if (key.shift && !selecting_) {
            selecting_ = true;
            sel_anchor_line_ = cursor_line_;
            sel_anchor_col_ = cursor_col_;
        } else if (!key.shift) {
            selecting_ = false;
        }
    }

    if (key.ctrl && key.key_code == 65) {
        handle_select_all();
    } else if (key.ctrl && key.key_code == 67) {
        handle_copy();
    } else if (key.ctrl && key.key_code == 88) {
        handle_cut();
    } else if (key.ctrl && key.key_code == 86) {
        handle_paste();
    } else if (key.ctrl && key.key_code == 83) {
        if (on_save) on_save();
    } else if (key.key_code == 13) {
        insert_codepoint('\n');
        suppress_char_after_enter_ = true;
    } else if (key.key_code == 8) {
        do_backspace();
    } else if (key.key_code == 46) {
        do_delete();
    } else if (key.key_code == 37) {
        if (key.ctrl) cursor_word_left(); else cursor_left();
    } else if (key.key_code == 39) {
        if (key.ctrl) cursor_word_right(); else cursor_right();
    } else if (key.key_code == 38) {
        cursor_up();
    } else if (key.key_code == 40) {
        cursor_down();
    } else if (key.key_code == 36) {
        cursor_home();
    } else if (key.key_code == 35) {
        cursor_end();
    } else if (key.codepoint >= 32 && !suppress_char_after_enter_) {
        insert_codepoint(key.codepoint);
    }

    cursor_visible_ = true;
    cursor_timer_ = 0.0f;
    ensure_cursor_visible();
    if (request_repaint_) request_repaint_();
}

void text_area::delete_selection() {
    if (!selecting_) return;

    size_t sl, sc, el, ec;
    normalize_selection(cursor_line_, cursor_col_,
                        sel_anchor_line_, sel_anchor_col_, sl, sc, el, ec);

    if (line_starts_dirty_) rebuild_line_starts();
    size_t start_pos = line_starts_[sl] + sc;
    size_t end_pos = line_starts_[el] + ec;
    text.erase(start_pos, end_pos - start_pos);
    line_starts_dirty_ = true;

    cursor_line_ = sl;
    cursor_col_ = sc;
    selecting_ = false;

    if (on_changed) on_changed(text);
}

void text_area::insert_codepoint(unsigned int cp) {
    if (selecting_) delete_selection();

    std::string utf8 = codepoint_to_utf8(cp);
    if (line_starts_dirty_) rebuild_line_starts();
    size_t pos = line_starts_[cursor_line_] + cursor_col_;
    text.insert(pos, utf8);
    line_starts_dirty_ = true;

    if (cp == '\n') {
        cursor_line_++;
        cursor_col_ = 0;
    } else {
        cursor_col_ += utf8.size();
    }

    if (on_changed) on_changed(text);
}

void text_area::do_backspace() {
    if (selecting_) { delete_selection(); return; }

    if (line_starts_dirty_) rebuild_line_starts();

    if (cursor_col_ > 0) {
        std::string_view sv = line_text(cursor_line_);
        size_t start = cursor_col_;
        while (start > 0 && (static_cast<unsigned char>(sv[start - 1]) & 0xC0) == 0x80)
            --start;
        --start;
        size_t pos = line_starts_[cursor_line_] + start;
        size_t end = line_starts_[cursor_line_] + cursor_col_;
        text.erase(pos, end - pos);
        line_starts_dirty_ = true;
        cursor_col_ = start;
    } else if (cursor_line_ > 0) {
        size_t prev_off = line_starts_[cursor_line_ - 1];
        size_t curr_off = line_starts_[cursor_line_];
        size_t prev_len = line_text(cursor_line_ - 1).size();
        text.erase(prev_off + prev_len, curr_off - prev_off - prev_len);
        line_starts_dirty_ = true;
        cursor_line_--;
        cursor_col_ = prev_len;
    }

    if (on_changed) on_changed(text);
}

void text_area::do_delete() {
    if (selecting_) { delete_selection(); return; }

    if (line_starts_dirty_) rebuild_line_starts();

    std::string_view sv = line_text(cursor_line_);
    if (cursor_col_ < sv.size()) {
        size_t end = cursor_col_ + 1;
        while (end < sv.size() && (static_cast<unsigned char>(sv[end]) & 0xC0) == 0x80)
            ++end;
        size_t pos = line_starts_[cursor_line_] + cursor_col_;
        size_t end_pos = line_starts_[cursor_line_] + end;
        text.erase(pos, end_pos - pos);
        line_starts_dirty_ = true;
    } else if (cursor_line_ + 1 < line_count()) {
        size_t curr_end = line_starts_[cursor_line_ + 1];
        text.erase(line_starts_[cursor_line_] + cursor_col_,
                   curr_end - line_starts_[cursor_line_] - cursor_col_);
        line_starts_dirty_ = true;
    }

    if (on_changed) on_changed(text);
}

std::string text_area::get_selected_text() const {
    if (!selecting_) return {};
    size_t sl, sc, el, ec;
    normalize_selection(cursor_line_, cursor_col_,
                        sel_anchor_line_, sel_anchor_col_, sl, sc, el, ec);
    if (line_starts_dirty_) const_cast<text_area*>(this)->rebuild_line_starts();
    size_t start = line_starts_[sl] + sc;
    size_t end = line_starts_[el] + ec;
    return text.substr(start, end - start);
}

void text_area::handle_select_all() {
    if (line_count() == 0) return;
    cursor_line_ = line_count() - 1;
    cursor_col_ = line_text(cursor_line_).size();
    sel_anchor_line_ = 0;
    sel_anchor_col_ = 0;
    selecting_ = true;
}

void text_area::handle_copy() {
    if (!selecting_) return;
    clipboard::copy(get_selected_text());
}

void text_area::handle_cut() {
    if (!selecting_) return;
    clipboard::copy(get_selected_text());
    delete_selection();
}

void text_area::handle_paste() {
    std::string t = clipboard::paste();
    if (t.empty()) return;

    size_t pos_r;
    while ((pos_r = t.find('\r')) != std::string::npos) t.erase(pos_r, 1);
    if (t.empty()) return;

    if (selecting_) delete_selection();
    if (line_starts_dirty_) rebuild_line_starts();
    size_t pos = line_starts_[cursor_line_] + cursor_col_;
    text.insert(pos, t);
    line_starts_dirty_ = true;

    size_t last_nl = t.rfind('\n');
    if (last_nl == std::string::npos) {
        cursor_col_ += t.size();
    } else {
        cursor_line_ += static_cast<size_t>(std::count(t.begin(), t.end(), '\n'));
        cursor_col_ = t.size() - last_nl - 1;
    }

    if (on_changed) on_changed(text);
}

void text_area::cursor_left() {
    if (cursor_col_ > 0) {
        std::string_view sv = line_text(cursor_line_);
        size_t new_col = cursor_col_ - 1;
        while (new_col > 0 && (static_cast<unsigned char>(sv[new_col]) & 0xC0) == 0x80)
            --new_col;
        cursor_col_ = new_col;
    } else if (cursor_line_ > 0) {
        cursor_line_--;
        cursor_col_ = line_text(cursor_line_).size();
    }
}

void text_area::cursor_right() {
    std::string_view sv = line_text(cursor_line_);
    if (cursor_col_ < sv.size()) {
        size_t new_col = cursor_col_ + 1;
        while (new_col < sv.size() && (static_cast<unsigned char>(sv[new_col]) & 0xC0) == 0x80)
            ++new_col;
        cursor_col_ = new_col;
    } else if (cursor_line_ + 1 < line_count()) {
        cursor_line_++;
        cursor_col_ = 0;
    }
}

void text_area::cursor_up() {
    if (cursor_line_ > 0) {
        cursor_line_--;
        clamp_cursor();
    }
}

void text_area::cursor_down() {
    if (cursor_line_ + 1 < line_count()) {
        cursor_line_++;
        clamp_cursor();
    }
}

void text_area::cursor_home() {
    cursor_col_ = 0;
}

void text_area::cursor_end() {
    cursor_col_ = line_text(cursor_line_).size();
}

void text_area::cursor_word_left() {
    std::string_view sv = line_text(cursor_line_);
    if (cursor_col_ == 0) {
        if (cursor_line_ > 0) { cursor_line_--; cursor_col_ = line_text(cursor_line_).size(); }
        return;
    }
    size_t pos = cursor_col_;
    while (pos > 0 && !is_word_char(sv[pos - 1])) --pos;
    while (pos > 0 && is_word_char(sv[pos - 1])) --pos;
    cursor_col_ = pos;
}

void text_area::cursor_word_right() {
    std::string_view sv = line_text(cursor_line_);
    if (cursor_col_ >= sv.size()) {
        if (cursor_line_ + 1 < line_count()) { cursor_line_++; cursor_col_ = 0; }
        return;
    }
    size_t pos = cursor_col_;
    while (pos < sv.size() && is_word_char(sv[pos])) ++pos;
    while (pos < sv.size() && !is_word_char(sv[pos])) ++pos;
    cursor_col_ = std::min(pos, sv.size());
}

void text_area::select_word_at(size_t line, size_t col, std::string_view sv) {
    if (sv.empty() || col >= sv.size()) return;
    if (!is_word_char(sv[col])) { selecting_ = false; return; }
    size_t start = col;
    while (start > 0 && is_word_char(sv[start - 1])) --start;
    size_t end = col;
    while (end < sv.size() && is_word_char(sv[end])) ++end;
    cursor_line_ = line;
    sel_anchor_line_ = line;
    cursor_col_ = end;
    sel_anchor_col_ = start;
    selecting_ = true;
}

void text_area::clamp_cursor() {
    size_t len = line_text(cursor_line_).size();
    if (cursor_col_ > len) cursor_col_ = len;
}

void text_area::ensure_cursor_visible() {
    size_t n = line_count();
    if (n == 0) return;

    float cursor_y = static_cast<float>(cursor_line_) * line_height;
    if (cursor_y < scroll_y_) {
        scroll_y_ = cursor_y;
    } else if (cursor_y + line_height > scroll_y_ + height - text_pad_top) {
        scroll_y_ = cursor_y + line_height - height + text_pad_top;
    }
    float max_scroll_y = std::max(0.0f,
        static_cast<float>(n) * line_height + text_pad_top - height);
    scroll_y_ = std::max(0.0f, std::min(scroll_y_, max_scroll_y));

    float cursor_x = cursor_pixel_x_;
    if (cursor_x < 0.0f) cursor_x = static_cast<float>(cursor_col_) * font_size * 0.6f;
    float view_w = std::max(width - text_x() - text_pad_right, 1.0f);
    if (cursor_x < scroll_x_) {
        scroll_x_ = cursor_x > 20.0f ? cursor_x - 20.0f : 0.0f;
    } else if (cursor_x > scroll_x_ + view_w - 10.0f) {
        scroll_x_ = cursor_x - view_w + 30.0f;
    }
    if (!max_width_dirty_) scroll_x_ = std::min(scroll_x_, scroll_max_x());
    scroll_x_ = std::max(0.0f, scroll_x_);
}

void text_area::resolve_position(float px, float py, size_t& out_line, size_t& out_col,
                                 std::shared_ptr<renderer> r) const {
    float click_y = py - text_pad_top + scroll_y_;
    float click_x = px - text_x() + scroll_x_;

    size_t n = line_count();
    out_line = 0;
    if (click_y > 0) {
        out_line = static_cast<size_t>(click_y / line_height);
        if (out_line >= n) out_line = n > 0 ? n - 1 : 0;
    }

    out_col = 0;
    if (click_x > 0) {
        std::string_view sv = line_text(out_line);
        std::string line_str(sv.data(), sv.size());

        size_t best_col = 0;
        float best_diff = click_x;
        size_t pos = 0;
        while (pos <= line_str.size()) {
            std::string prefix = line_str.substr(0, pos);
            float w = r->measure_text_width(prefix, font_size, font_family);
            float diff = std::abs(w - click_x);
            if (diff < best_diff) {
                best_diff = diff;
                best_col = pos;
            }
            if (pos >= line_str.size()) break;
            unsigned char c = static_cast<unsigned char>(line_str[pos]);
            size_t char_bytes = 1;
            if ((c & 0xF0) == 0xF0) char_bytes = 4;
            else if ((c & 0xE0) == 0xE0) char_bytes = 3;
            else if ((c & 0xC0) == 0xC0) char_bytes = 2;
            pos += char_bytes;
        }
        out_col = best_col;
    }
}

void text_area::resolve_click(std::shared_ptr<renderer> r) {
    pending_click_.active = false;

    size_t resolved_line = 0, resolved_col = 0;
    resolve_position(pending_click_.x, pending_click_.y, resolved_line, resolved_col, r);

    if (pending_click_.dbl) {
        cursor_line_ = resolved_line;
        cursor_col_ = resolved_col;
        std::string_view sv = line_text(cursor_line_);
        select_word_at(cursor_line_, cursor_col_, sv);
    } else if (pending_is_drag_) {
        pending_is_drag_ = false;
        if (!drag_resolved_) {
            drag_resolved_ = true;
            selecting_ = true;
            // 拖选锚点固定为按下位置（避免首次 resolve 已是拖选时 anchor 丢失）
            resolve_position(press_x_, press_y_, sel_anchor_line_, sel_anchor_col_, r);
        }
        cursor_line_ = resolved_line;
        cursor_col_ = resolved_col;
    } else {
        if (pending_click_.shift) {
            if (!selecting_) {
                sel_anchor_line_ = cursor_line_;
                sel_anchor_col_ = cursor_col_;
            }
            selecting_ = true;
            cursor_line_ = resolved_line;
            cursor_col_ = resolved_col;
        } else {
            cursor_line_ = resolved_line;
            cursor_col_ = resolved_col;
            selecting_ = false;
            sel_anchor_line_ = resolved_line;
            sel_anchor_col_ = resolved_col;
        }
    }

    cursor_visible_ = true;
    cursor_timer_ = 0.0f;
}

void text_area::paint(std::shared_ptr<renderer> r) {
    if (pending_click_.active) resolve_click(r);
    if (max_width_dirty_) {
        compute_max_line_width(r);
        scroll_x_ = std::max(0.0f, std::min(scroll_x_, scroll_max_x()));
    }

    draw_background(r);

    size_t total_lines = line_count();

    if (total_lines > 0) {
        size_t first_visible = static_cast<size_t>(scroll_y_ / line_height);
        float view_height = std::max(height, 100.0f);
        size_t max_visible = static_cast<size_t>(view_height / line_height) + 2;
        float y_offset = text_pad_top - std::fmod(scroll_y_, line_height);

        if (selecting_) draw_selection(r);

        for (size_t i = first_visible; i < first_visible + max_visible && i < total_lines; ++i) {
            float line_y = y_offset + static_cast<float>(i - first_visible) * line_height;
            if (line_y < -line_height || line_y > height) continue;
            draw_line_content(r, i, line_y);
        }
    }

    if (show_line_numbers) draw_gutter(r);

    draw_cursor(r);
    draw_scrollbar(r);
}

void text_area::draw_background(std::shared_ptr<renderer> r) {
    r->draw_rectangle({0, 0, width, height}, theme_manager::get(theme_manager::WINDOW_BG));
}

void text_area::draw_gutter(std::shared_ptr<renderer> r) {
    if (!show_line_numbers) return;
    r->draw_rectangle({0, 0, gutter_width, height},
                      theme_manager::get(theme_manager::TAB_INACTIVE_BG));
    r->draw_line({gutter_width, 0}, {gutter_width, height},
                 theme_manager::get(theme_manager::SEPARATOR), 1.0f);

    size_t total_lines = line_count();
    if (total_lines == 0) return;
    size_t first_visible = static_cast<size_t>(scroll_y_ / line_height);
    float view_height = std::max(height, 100.0f);
    size_t max_visible = static_cast<size_t>(view_height / line_height) + 2;
    float y_offset = text_pad_top - std::fmod(scroll_y_, line_height);
    for (size_t i = first_visible; i < first_visible + max_visible && i < total_lines; ++i) {
        float line_y = y_offset + static_cast<float>(i - first_visible) * line_height;
        if (line_y < -line_height || line_y > height) continue;
        draw_line_numbers(r, i, line_y);
    }
}

void text_area::draw_line_numbers(std::shared_ptr<renderer> r, size_t line_idx, float line_y) {
    std::string num = std::to_string(line_idx + 1);
    float text_width = r->measure_text_width(num, font_size - 1.0f, font_family);
    float num_x = gutter_width - 10.0f - text_width;
    r->draw_text(num, {num_x, line_y + 2.0f},
                 theme_manager::get(theme_manager::TAB_INACTIVE_TEXT),
                 font_size - 1.0f, font_family);
}

void text_area::draw_line_content(std::shared_ptr<renderer> r, size_t line_idx, float line_y) {
    std::string_view sv = line_text(line_idx);
    if (sv.empty()) return;

    float text_x_pos = text_x() - scroll_x_;
    float text_y = line_y + 2.0f;
    r->draw_text(std::string(sv.data(), sv.size()), {text_x_pos, text_y},
                 theme_manager::get(theme_manager::INPUT_TEXT), font_size, font_family, false);
}

void text_area::draw_cursor(std::shared_ptr<renderer> r) {
    if (!cursor_visible_) return;

    std::string_view sv = line_text(cursor_line_);
    std::string prefix(sv.data(), std::min(cursor_col_, sv.size()));
    cursor_pixel_x_ = r->measure_text_width(prefix, font_size, font_family);

    float cx = text_x() + cursor_pixel_x_ - scroll_x_;
    float cy = text_pad_top + static_cast<float>(cursor_line_) * line_height - scroll_y_;

    if (cy + line_height < 0 || cy > height) return;
    if (cx > width || cx + 1.0f < 0) return;

    r->draw_rectangle({cx, cy, 1.0f, line_height},
                      theme_manager::get(theme_manager::CONTROL_ICON_HOVER));
}

void text_area::draw_selection(std::shared_ptr<renderer> r) {
    if (!selecting_) return;

    size_t sl, sc, el, ec;
    normalize_selection(cursor_line_, cursor_col_,
                        sel_anchor_line_, sel_anchor_col_, sl, sc, el, ec);

    color sel_color = {0.20f, 0.40f, 0.80f, 0.35f};

    for (size_t line = sl; line <= el; ++line) {
        std::string_view sv = line_text(line);
        if (sv.empty()) continue;

        size_t col_start = (line == sl) ? sc : 0;
        size_t col_end = (line == el) ? ec : sv.size();

        std::string before(sv.data(), col_start);
        std::string selected(sv.data() + col_start, col_end - col_start);

        float tx = text_x() + r->measure_text_width(before, font_size, font_family) - scroll_x_;
        float sel_w = r->measure_text_width(selected, font_size, font_family);
        float ly = text_pad_top + static_cast<float>(line) * line_height - scroll_y_;

        r->draw_rectangle({tx, ly, std::max(sel_w, 1.0f), line_height}, sel_color);
    }
}

bool text_area::is_on_scrollbar(float px, float py) const {
    return px >= width - scroll_bar_width && px < width &&
           py >= 0 && py < height;
}

bool text_area::is_on_scrollbar_thumb(float px, float py) const {
    float ty, th, ms;
    scrollbar_thumb_rect(ty, th, ms);
    if (th <= 0.0f) return false;
    return px >= width - scroll_bar_width && px < width &&
           py >= ty && py < ty + th;
}

void text_area::scrollbar_thumb_rect(float& out_y, float& out_h, float& out_max_scroll) const {
    size_t n = line_count();
    if (n == 0) { out_y = 0; out_h = 0; out_max_scroll = 0; return; }
    float content_h = static_cast<float>(n) * line_height + text_pad_top;
    float view_h = height - (h_scroll_visible() ? scroll_bar_width : 0.0f);
    out_max_scroll = std::max(0.0f, content_h - view_h);
    if (out_max_scroll <= 0.0f) { out_y = 0; out_h = 0; return; }
    float thumb_ratio = view_h / content_h;
    out_h = std::max(view_h * thumb_ratio, 4.0f);
    if (out_h > view_h) out_h = view_h;
    float thumb_range = view_h - out_h;
    out_y = (scroll_y_ / out_max_scroll) * thumb_range;
}

bool text_area::is_on_h_scrollbar_thumb(float px, float py) const {
    float hx, hw, hms;
    h_scrollbar_thumb_rect(hx, hw, hms);
    if (hw <= 0.0f) return false;
    return py >= height - scroll_bar_width && py < height &&
           px >= text_x() + hx && px < text_x() + hx + hw;
}

void text_area::h_scrollbar_thumb_rect(float& out_x, float& out_w, float& out_max_scroll) const {
    float view_w = std::max(1.0f, width - text_x());
    float content_w = max_line_width_ + text_pad_right;
    out_max_scroll = std::max(0.0f, content_w - view_w);
    if (out_max_scroll <= 0.0f) { out_x = 0; out_w = 0; return; }
    float thumb_ratio = view_w / content_w;
    out_w = std::max(view_w * thumb_ratio, 4.0f);
    if (out_w > view_w) out_w = view_w;
    float thumb_range = view_w - out_w;
    out_x = (scroll_x_ / out_max_scroll) * thumb_range;
}

void text_area::draw_scrollbar(std::shared_ptr<renderer> r) {
    float ty, th, ms;
    scrollbar_thumb_rect(ty, th, ms);
    if (th > 0.0f && ms > 0.0f) {
        float sb_x = width - scroll_bar_width;
        float track_h = height - (h_scroll_visible() ? scroll_bar_width : 0.0f);
        r->draw_rectangle({sb_x, 0, scroll_bar_width, track_h}, {0.12f, 0.12f, 0.12f, 0.7f});
        r->draw_rectangle({sb_x, ty, scroll_bar_width, th}, scrollbar_thumb_bg_.current());
    }

    float hx, hw, hms;
    h_scrollbar_thumb_rect(hx, hw, hms);
    if (hw > 0.0f && hms > 0.0f) {
        float sb_y = height - scroll_bar_width;
        float sb_x = text_x();
        float track_w = width - sb_x;
        r->draw_rectangle({sb_x, sb_y, track_w, scroll_bar_width}, {0.12f, 0.12f, 0.12f, 0.7f});
        r->draw_rectangle({sb_x + hx, sb_y, hw, scroll_bar_width}, h_scrollbar_thumb_bg_.current());
    }
}

size text_area::layout_preferred_size() const {
    return {width, height};
}

} // namespace spiration
