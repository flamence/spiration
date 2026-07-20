#include <extension/builtin/edit/edit_tab.h>
#include <ui/theme_manager.h>
#include <renderer/renderer.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace spiration {
namespace edit {

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
static bool is_shift_down()    { return (GetKeyState(VK_SHIFT) & 0x8000) != 0; }
static bool is_ctrl_down()     { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }
static bool is_mouse_left_down() { return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; }

static void clipboard_copy(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, wlen * 2);
    if (h) {
        wchar_t* dst = (wchar_t*)GlobalLock(h);
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, dst, wlen);
        GlobalUnlock(h);
        if (!SetClipboardData(CF_UNICODETEXT, h)) {
            GlobalFree(h);
        }
    }
    CloseClipboard();
}

static std::string clipboard_paste() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return {};
    if (!OpenClipboard(nullptr)) return {};
    std::string result;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        wchar_t* src = (wchar_t*)GlobalLock(h);
        if (src) {
            int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, nullptr, 0, nullptr, nullptr);
            result.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, src, -1, result.data(), len, nullptr, nullptr);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return result;
}
#else
static bool is_shift_down()    { return false; }
static bool is_ctrl_down()     { return false; }
static bool is_mouse_left_down() { return false; }
static void clipboard_copy(const std::string&) {}
static std::string clipboard_paste() { return {}; }
#endif

static long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool is_word_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static std::string codepoint_to_utf8(unsigned int cp) {
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

static const color TEXT_COLOR = {1.0f, 1.0f, 1.0f};

void edit_tab::rebuild_line_starts() {
    line_starts_.clear();
    line_starts_.push_back(0);
    for (size_t i = 0; i < buffer_.size(); ++i) {
        if (buffer_[i] == '\n') {
            line_starts_.push_back(i + 1);
        }
    }
    line_starts_.push_back(buffer_.size());
    line_starts_dirty_ = false;
}

size_t edit_tab::line_count() const {
    if (line_starts_dirty_) {
        const_cast<edit_tab*>(this)->rebuild_line_starts();
    }
    if (line_starts_.size() <= 1) return 0;
    return line_starts_.size() - 1;
}

std::string_view edit_tab::line_text(size_t n) const {
    if (line_starts_dirty_) {
        const_cast<edit_tab*>(this)->rebuild_line_starts();
    }
    if (n >= line_starts_.size() - 1) return {};
    size_t start = line_starts_[n];
    size_t end = line_starts_[n + 1];
    if (end > start) {
        if (buffer_[end - 1] == '\n') --end;
        if (end > start && buffer_[end - 1] == '\r') --end;
    }
    return std::string_view(buffer_.data() + start, end - start);
}

static size_t buffer_insert(std::string& buf, const std::vector<size_t>& starts,
                            size_t line, size_t col, const std::string& text) {
    if (line >= starts.size()) return 0;
    size_t line_len = (line + 1 < starts.size()) ? starts[line + 1] - starts[line] : buf.size() - starts[line];
    if (col > line_len) col = line_len;
    size_t pos = starts[line] + col;
    if (pos > buf.size()) pos = buf.size();
    buf.insert(pos, text);
    return text.size();
}

static size_t buffer_erase(std::string& buf, const std::vector<size_t>& starts,
                           size_t sl, size_t sc, size_t el, size_t ec) {
    if (sl >= starts.size() || el >= starts.size()) return 0;
    size_t start = starts[sl] + sc;
    size_t end = starts[el] + ec;
    if (start > buf.size()) start = buf.size();
    if (end > buf.size()) end = buf.size();
    if (end <= start) return 0;
    size_t len = end - start;
    buf.erase(start, len);
    return len;
}

static void normalize_selection(size_t cl, size_t cc, size_t al, size_t ac,
                                size_t& sl, size_t& sc, size_t& el, size_t& ec) {
    if (cl < al || (cl == al && cc < ac)) {
        sl = cl; sc = cc; el = al; ec = ac;
    } else {
        sl = al; sc = ac; el = cl; ec = cc;
    }
}

edit_tab::edit_tab()
    : scrollbar_thumb_bg_({0.45f, 0.45f, 0.45f, 0.5f}) {
    base_title_ = "Untitled";
    title_ = base_title_;
    widget_style.background_color = theme_manager::get(theme_manager::WINDOW_BG);
    last_click_ms_ = now_ms() - DBL_CLICK_MS * 2;
    line_starts_dirty_ = true;
}

edit_tab::edit_tab(const std::string& title)
    : scrollbar_thumb_bg_({0.45f, 0.45f, 0.45f, 0.5f}) {
    base_title_ = title;
    title_ = base_title_;
    widget_style.background_color = theme_manager::get(theme_manager::WINDOW_BG);
    last_click_ms_ = now_ms() - DBL_CLICK_MS * 2;
    line_starts_dirty_ = true;
}

void edit_tab::mark_dirty() {
    if (!dirty_) { dirty_ = true; update_title(); }
}

void edit_tab::mark_clean() {
    if (dirty_) dirty_ = false;
    update_title();
}

void edit_tab::update_title() {
    title_ = dirty_ ? "*" + base_title_ : base_title_;
    if (on_title_change_) on_title_change_(title_);
}

void edit_tab::set_text(const std::string& text) {
    buffer_ = text;
    line_starts_dirty_ = true;
    cursor_line_ = 0;
    cursor_col_ = 0;
    selecting_ = false;
    scroll_y_ = 0.0f;
    scroll_x_ = 0.0f;
    mark_clean();
}

void edit_tab::load_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return;

    std::streampos file_size = file.tellg();
    if (file_size > static_cast<std::streampos>(MAX_FILE_SIZE)) {
        return;
    }
    file.seekg(0, std::ios::beg);

    std::string buf;
    buf.resize(static_cast<size_t>(file_size));

    constexpr size_t CHUNK = 65536;
    char* dst = buf.data();
    std::streamsize remaining = static_cast<std::streamsize>(buf.size());
    while (remaining > 0) {
        std::streamsize to_read = std::min(remaining, static_cast<std::streamsize>(CHUNK));
        file.read(dst, to_read);
        std::streamsize read = file.gcount();
        if (read <= 0) break;
        dst += read;
        remaining -= read;
    }

    file.close();
    buf.resize(static_cast<size_t>(dst - buf.data()));

    size_t sep = path.find_last_of("/\\");
    base_title_ = (sep != std::string::npos) ? path.substr(sep + 1) : path;
    title_ = base_title_;

    buffer_ = std::move(buf);
    file_path_ = path;
    line_starts_dirty_ = true;
    cursor_line_ = 0;
    cursor_col_ = 0;
    selecting_ = false;
    scroll_y_ = 0.0f;
    scroll_x_ = 0.0f;
    mark_clean();
}

bool edit_tab::save() {
    if (file_path_.empty()) return false;
    return save_as(file_path_);
}

bool edit_tab::save_as(const std::string& path) {
    std::string tmp_path = path + ".tmp";
    std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);
    if (!file) return false;

    constexpr size_t CHUNK = 65536;
    size_t remaining = buffer_.size();
    size_t offset = 0;
    while (remaining > 0) {
        size_t to_write = std::min(remaining, static_cast<size_t>(CHUNK));
        file.write(buffer_.data() + offset, static_cast<std::streamsize>(to_write));
        if (!file) {
            file.close();
            std::remove(tmp_path.c_str());
            return false;
        }
        offset += to_write;
        remaining -= to_write;
    }

    file.close();

    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::remove(tmp_path.c_str());
        return false;
    }

    file_path_ = path;

    size_t sep = path.find_last_of("/\\");
    base_title_ = (sep != std::string::npos) ? path.substr(sep + 1) : path;
    mark_clean();

    return true;
}
void edit_tab::paint(std::shared_ptr<renderer> r) {
    if (pending_click_.active) resolve_click(r);

    draw_background(r);

    float avail_width = width - LEFT_MARGIN - RIGHT_PAD - SCROLLBAR_W;
    size_t total_lines = line_count();
    if (total_lines == 0) {
        draw_cursor(r);
        draw_scrollbar(r);
        return;
    }

    size_t first_visible = static_cast<size_t>(scroll_y_ / LINE_HEIGHT);
    float view_height = std::max(height, 100.0f);
    size_t max_visible = static_cast<size_t>(view_height / LINE_HEIGHT) + 2;
    float y_offset = TOP_PAD - std::fmod(scroll_y_, LINE_HEIGHT);

    if (selecting_) draw_selection(r);

    for (size_t i = first_visible; i < first_visible + max_visible && i < total_lines; ++i) {
        float line_y = y_offset + static_cast<float>(i - first_visible) * LINE_HEIGHT;
        if (line_y < -LINE_HEIGHT || line_y > height) continue;

        draw_line_numbers(r, i, line_y);
        draw_line_content(r, i, line_y, avail_width);
    }

    draw_cursor(r);
    draw_scrollbar(r);
}

void edit_tab::resolve_click(std::shared_ptr<renderer> r) {
    pending_click_.active = false;

    float click_y = pending_click_.y - TOP_PAD + scroll_y_;
    float click_x = pending_click_.x - LEFT_MARGIN - 8.0f + scroll_x_;

    size_t n = line_count();
    size_t resolved_line = 0;
    if (click_y > 0) {
        resolved_line = static_cast<size_t>(click_y / LINE_HEIGHT);
        if (resolved_line >= n) resolved_line = n > 0 ? n - 1 : 0;
    }

    size_t resolved_col = 0;
    if (click_x > 0) {
        std::string_view sv = line_text(resolved_line);
        std::string line_str(sv.data(), sv.size());

        size_t best_col = 0;
        float best_diff = click_x;
        size_t pos = 0;
        while (pos <= line_str.size()) {
            std::string prefix = line_str.substr(0, pos);
            float w = r->measure_text_width(prefix, 15.0f, "Consolas");
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
        resolved_col = best_col;
    }

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

void edit_tab::draw_background(std::shared_ptr<renderer> r) {
    r->draw_rectangle({x, y, width, height}, theme_manager::get(theme_manager::WINDOW_BG));
    r->draw_rectangle({x, y, LEFT_MARGIN, height}, theme_manager::get(theme_manager::TAB_INACTIVE_BG));
    r->draw_line({x + LEFT_MARGIN, y}, {x + LEFT_MARGIN, y + height},
                 theme_manager::get(theme_manager::SEPARATOR), 1.0f);
}

void edit_tab::draw_line_numbers(std::shared_ptr<renderer> r, size_t line_idx, float line_y) {
    std::string num = std::to_string(line_idx + 1);
    float text_width = r->measure_text_width(num, 14.0f, "Consolas");
    float num_x = x + LEFT_MARGIN - 10.0f - text_width;
    r->draw_text(num, {num_x, y + line_y + 2.0f},
                 theme_manager::get(theme_manager::TAB_INACTIVE_TEXT), 14.0f, "Consolas");
}

void edit_tab::draw_line_content(std::shared_ptr<renderer> r, size_t line_idx,
                                  float line_y, float /*avail_width*/) {
    std::string_view sv = line_text(line_idx);
    if (sv.empty()) return;

    float text_x = x + LEFT_MARGIN + 8.0f - scroll_x_;
    float text_y = y + line_y + 2.0f;
    r->draw_text(std::string(sv.data(), sv.size()), {text_x, text_y},
                 TEXT_COLOR, 15.0f, "Consolas", false);
}

void edit_tab::draw_cursor(std::shared_ptr<renderer> r) {
    if (!cursor_visible_) return;

    std::string_view sv = line_text(cursor_line_);
    std::string prefix(sv.data(), std::min(cursor_col_, sv.size()));
    cursor_pixel_x_ = r->measure_text_width(prefix, 15.0f, "Consolas");

    float cx = x + LEFT_MARGIN + 8.0f + cursor_pixel_x_ - scroll_x_;
    float cy = y + TOP_PAD + cursor_line_ * LINE_HEIGHT - scroll_y_;

    if (cy + LINE_HEIGHT < y || cy > y + height) return;
    if (cx > x + width || cx + 1.0f < x) return;

    r->draw_rectangle({cx, cy, 1.0f, LINE_HEIGHT}, theme_manager::get(theme_manager::CONTROL_ICON_HOVER));
}

void edit_tab::draw_selection(std::shared_ptr<renderer> r) {
    if (!selecting_) return;

    size_t sl, sc, el, ec;
    normalize_selection(cursor_line_, cursor_col_,
                        sel_anchor_line_, sel_anchor_col_,
                        sl, sc, el, ec);

    color sel_color = {0.20f, 0.40f, 0.80f, 0.35f};

    for (size_t line = sl; line <= el; ++line) {
        std::string_view sv = line_text(line);
        if (sv.empty()) continue;

        size_t col_start = (line == sl) ? sc : 0;
        size_t col_end = (line == el) ? ec : sv.size();

        std::string before(sv.data(), col_start);
        std::string selected(sv.data() + col_start, col_end - col_start);

        float tx = x + LEFT_MARGIN + 8.0f + r->measure_text_width(before, 15.0f, "Consolas") - scroll_x_;
        float sel_w = r->measure_text_width(selected, 15.0f, "Consolas");
        float ly = y + TOP_PAD + line * LINE_HEIGHT - scroll_y_;

        r->draw_rectangle({tx, ly, std::max(sel_w, 1.0f), LINE_HEIGHT}, sel_color);
    }
}

void edit_tab::handle_event(const event_type& type, void* data) {
    if (!data) return;
    if (type == event_type::keyboard) {
        auto* key = static_cast<key_event_data*>(data);
        handle_key(*key);
    } else if (type == event_type::mouse) {
        auto* mouse = static_cast<mouse_event_data*>(data);
        if (mouse->action == mouse_action::down &&
            mouse->button == mouse_button::left) {
            if (is_on_scrollbar_thumb(mouse->position.x, mouse->position.y)) {
                scrollbar_dragging_ = true;
                scrollbar_drag_start_y_ = mouse->position.y;
                scrollbar_drag_start_scroll_ = scroll_y_;
                set_mouse_capture(this);
                mouse->consumed = true;
                if (repaint_cb_) repaint_cb_();
                return;
            }
            long long t = now_ms();
            pending_click_.dbl = (t - last_click_ms_ < DBL_CLICK_MS);
            last_click_ms_ = t;

            mouse_down_ = false;
            selecting_ = false;
            mouse_down_ = true;
            pending_is_drag_ = false;
            drag_resolved_ = false;
            pending_click_.x = mouse->position.x;
            pending_click_.y = mouse->position.y;
            pending_click_.shift = is_shift_down();
            pending_click_.active = true;
            set_mouse_capture(this);
            mouse->consumed = true;
            if (repaint_cb_) repaint_cb_();
        } else if (mouse->action == mouse_action::move && (mouse_down_ || scrollbar_dragging_)) {
            if (scrollbar_dragging_) {
                float dy = mouse->position.y - scrollbar_drag_start_y_;
                float ty, th, ms;
                scrollbar_thumb_rect(ty, th, ms);
                if (ms > 0.0f) {
                    float range = ms;
                    scroll_y_ = scrollbar_drag_start_scroll_ + dy / (height - th) * range;
                    scroll_y_ = std::max(0.0f, std::min(scroll_y_, range));
                }
                if (repaint_cb_) repaint_cb_();
            } else if (mouse_down_) {
                pending_is_drag_ = true;
                pending_click_.x = mouse->position.x;
                pending_click_.y = mouse->position.y;
                pending_click_.active = true;
                mouse->consumed = true;
                if (repaint_cb_) repaint_cb_();
            }
        } else if (mouse->action == mouse_action::move) {
            bool over = is_on_scrollbar_thumb(mouse->position.x, mouse->position.y);
            if (over != scrollbar_hovering_) {
                scrollbar_hovering_ = over;
                if (over) {
                    scrollbar_thumb_bg_.animate_to({0.55f, 0.55f, 0.55f, 0.8f}, 100.0f);
                } else {
                    scrollbar_thumb_bg_.animate_to({0.45f, 0.45f, 0.45f, 0.5f}, 150.0f);
                }
                if (repaint_cb_) repaint_cb_();
            }
        } else if (mouse->action == mouse_action::up &&
                   mouse->button == mouse_button::left) {
            set_mouse_capture(nullptr);
            scrollbar_dragging_ = false;
            mouse_down_ = false;
            mouse->consumed = true;
        } else if (mouse->action == mouse_action::wheel) {
            if (is_shift_down()) {
                scroll_x_ -= mouse->wheel_delta * 6.0f;
                scroll_x_ = std::max(0.0f, scroll_x_);
                if (repaint_cb_) repaint_cb_();
            } else {
                scroll_y_ -= mouse->wheel_delta * 3.0f;
                float max_scroll = std::max(0.0f,
                    line_count() * LINE_HEIGHT + TOP_PAD - height);
                scroll_y_ = std::max(0.0f, std::min(scroll_y_, max_scroll));
            }
            mouse->consumed = true;
            if (repaint_cb_) repaint_cb_();
        }
    }
}

void edit_tab::tick(float dt_ms) {
    if (mouse_down_ && !is_mouse_left_down()) {
        mouse_down_ = false;
        selecting_ = false;
    }

    if (scrollbar_thumb_bg_.update(dt_ms) && repaint_cb_) repaint_cb_();

    cursor_timer_ += dt_ms;
    if (cursor_timer_ >= CURSOR_BLINK_MS) {
        cursor_visible_ = !cursor_visible_;
        cursor_timer_ = 0.0f;
        if (repaint_cb_) repaint_cb_();
    }
}

void edit_tab::handle_key(const key_event_data& key) {
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
        if (save_cb_) save_cb_();
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
    if (repaint_cb_) repaint_cb_();
}

void edit_tab::delete_selection() {
    if (!selecting_) return;

    size_t sl, sc, el, ec;
    normalize_selection(cursor_line_, cursor_col_,
                        sel_anchor_line_, sel_anchor_col_,
                        sl, sc, el, ec);

    if (line_starts_dirty_) rebuild_line_starts();
    size_t start_pos = line_starts_[sl] + sc;
    size_t end_pos = line_starts_[el] + ec;
    buffer_.erase(start_pos, end_pos - start_pos);
    line_starts_dirty_ = true;
    mark_dirty();

    cursor_line_ = sl;
    cursor_col_ = sc;
    selecting_ = false;
}

void edit_tab::insert_codepoint(unsigned int cp) {
    if (selecting_) delete_selection();

    std::string utf8 = codepoint_to_utf8(cp);
    if (line_starts_dirty_) rebuild_line_starts();
    size_t pos = line_starts_[cursor_line_] + cursor_col_;
    buffer_.insert(pos, utf8);
    line_starts_dirty_ = true;
    mark_dirty();
    if (cp == '\n') {
        cursor_line_++;
        cursor_col_ = 0;
    } else {
        cursor_col_ += utf8.size();
    }
}

void edit_tab::do_backspace() {
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
        buffer_.erase(pos, end - pos);
        line_starts_dirty_ = true;
        mark_dirty();
        cursor_col_ = start;
    } else if (cursor_line_ > 0) {
        size_t prev_off = line_starts_[cursor_line_ - 1];
        size_t curr_off = line_starts_[cursor_line_];
        std::string_view prev_sv = line_text(cursor_line_ - 1);
        size_t prev_len = prev_sv.size();
        buffer_.erase(prev_off + prev_len, curr_off - prev_off - prev_len);
        line_starts_dirty_ = true;
        mark_dirty();
        cursor_line_--;
        cursor_col_ = prev_len;
    }
}

void edit_tab::do_delete() {
    if (selecting_) { delete_selection(); return; }

    if (line_starts_dirty_) rebuild_line_starts();

    std::string_view sv = line_text(cursor_line_);
    if (cursor_col_ < sv.size()) {
        size_t end = cursor_col_ + 1;
        while (end < sv.size() && (static_cast<unsigned char>(sv[end]) & 0xC0) == 0x80)
            ++end;
        size_t pos = line_starts_[cursor_line_] + cursor_col_;
        size_t end_pos = line_starts_[cursor_line_] + end;
        buffer_.erase(pos, end_pos - pos);
        line_starts_dirty_ = true;
        mark_dirty();
    } else if (cursor_line_ + 1 < line_count()) {
        size_t curr_end = line_starts_[cursor_line_ + 1];
        buffer_.erase(line_starts_[cursor_line_] + cursor_col_,
                      curr_end - line_starts_[cursor_line_] - cursor_col_);
        line_starts_dirty_ = true;
        mark_dirty();
    }
}

std::string edit_tab::get_selected_text() const {
    if (!selecting_) return {};
    size_t sl, sc, el, ec;
    normalize_selection(cursor_line_, cursor_col_,
                        sel_anchor_line_, sel_anchor_col_,
                        sl, sc, el, ec);

    if (line_starts_dirty_) const_cast<edit_tab*>(this)->rebuild_line_starts();

    size_t start = line_starts_[sl] + sc;
    size_t end = line_starts_[el] + ec;
    return buffer_.substr(start, end - start);
}

void edit_tab::handle_select_all() {
    if (line_count() == 0) return;
    cursor_line_ = line_count() - 1;
    cursor_col_ = line_text(cursor_line_).size();
    sel_anchor_line_ = 0;
    sel_anchor_col_ = 0;
    selecting_ = true;
}

void edit_tab::handle_copy() {
    if (!selecting_) return;
    clipboard_copy(get_selected_text());
}

void edit_tab::handle_cut() {
    if (!selecting_) return;
    clipboard_copy(get_selected_text());
    delete_selection();
    mark_dirty();
}

void edit_tab::handle_paste() {
    std::string text = clipboard_paste();
    if (text.empty()) return;

    size_t pos_r;
    while ((pos_r = text.find('\r')) != std::string::npos) {
        if (pos_r + 1 < text.size() && text[pos_r + 1] == '\n') {
            text.erase(pos_r, 1);
        } else {
            text.erase(pos_r, 1);
        }
    }

    if (text.empty()) return;
    if (selecting_) delete_selection();
    if (line_starts_dirty_) rebuild_line_starts();
    size_t pos = line_starts_[cursor_line_] + cursor_col_;
    buffer_.insert(pos, text);
    line_starts_dirty_ = true;
    mark_dirty();
    size_t last_nl = text.rfind('\n');
    if (last_nl == std::string::npos) {
        cursor_col_ += text.size();
    } else {
        cursor_line_ += std::count(text.begin(), text.end(), '\n');
        cursor_col_ = text.size() - last_nl - 1;
    }
}

void edit_tab::cursor_left() {
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

void edit_tab::cursor_right() {
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

void edit_tab::cursor_up() {
    if (cursor_line_ > 0) {
        cursor_line_--;
        clamp_cursor();
    }
}

void edit_tab::cursor_down() {
    if (cursor_line_ + 1 < line_count()) {
        cursor_line_++;
        clamp_cursor();
    }
}

void edit_tab::cursor_home() {
    cursor_col_ = 0;
}

void edit_tab::cursor_end() {
    cursor_col_ = line_text(cursor_line_).size();
}

void edit_tab::cursor_word_left() {
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

void edit_tab::cursor_word_right() {
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

void edit_tab::select_word_at(size_t line, size_t col, std::string_view sv) {
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

void edit_tab::clamp_cursor() {
    size_t len = line_text(cursor_line_).size();
    if (cursor_col_ > len) cursor_col_ = len;
}

void edit_tab::ensure_cursor_visible() {
    size_t n = line_count();
    if (n == 0) return;

    float cursor_y = cursor_line_ * LINE_HEIGHT;
    if (cursor_y < scroll_y_) {
        scroll_y_ = cursor_y;
    } else if (cursor_y + LINE_HEIGHT > scroll_y_ + height - TOP_PAD) {
        scroll_y_ = cursor_y + LINE_HEIGHT - height + TOP_PAD;
    }
    float max_scroll_y = std::max(0.0f,
        static_cast<float>(n) * LINE_HEIGHT + TOP_PAD - height);
    scroll_y_ = std::max(0.0f, std::min(scroll_y_, max_scroll_y));

    float cursor_x = cursor_pixel_x_;
    if (cursor_x < 0.0f) cursor_x = static_cast<float>(cursor_col_) * 9.0f;
    float view_w = std::max(width - LEFT_MARGIN - RIGHT_PAD, 1.0f);
    if (cursor_x < scroll_x_) {
        scroll_x_ = cursor_x > 20.0f ? cursor_x - 20.0f : 0.0f;
    } else if (cursor_x > scroll_x_ + view_w - 10.0f) {
        scroll_x_ = cursor_x - view_w + 30.0f;
    }
    scroll_x_ = std::max(0.0f, scroll_x_);
}

bool edit_tab::is_on_scrollbar(float px, float py) const {
    return px >= x + width - SCROLLBAR_W && px < x + width &&
           py >= y && py < y + height;
}

bool edit_tab::is_on_scrollbar_thumb(float px, float py) const {
    float ty, th, ms;
    scrollbar_thumb_rect(ty, th, ms);
    if (th <= 0.0f) return false;
    return px >= x + width - SCROLLBAR_W && px < x + width &&
           py >= ty && py < ty + th;
}

void edit_tab::scrollbar_thumb_rect(float& out_y, float& out_h, float& out_max_scroll) const {
    size_t n = line_count();
    if (n == 0) { out_y = 0; out_h = 0; out_max_scroll = 0; return; }
    float content_h = static_cast<float>(n) * LINE_HEIGHT + TOP_PAD;
    float view_h = height;
    out_max_scroll = std::max(0.0f, content_h - view_h);
    if (out_max_scroll <= 0.0f) { out_y = 0; out_h = 0; return; }
    float thumb_ratio = view_h / content_h;
    out_h = std::max(height * thumb_ratio, 4.0f);
    if (out_h > height) out_h = height;
    float thumb_range = height - out_h;
    out_y = (scroll_y_ / out_max_scroll) * thumb_range;
}

void edit_tab::draw_scrollbar(std::shared_ptr<renderer> r) {
    float ty, th, ms;
    scrollbar_thumb_rect(ty, th, ms);
    if (th <= 0.0f) return;

    float sb_x = x + width - SCROLLBAR_W;
    color track = {0.12f, 0.12f, 0.12f, 0.7f};
    r->draw_rectangle({sb_x, y, SCROLLBAR_W, height}, track);
    r->draw_rectangle({sb_x, y + ty, SCROLLBAR_W, th}, scrollbar_thumb_bg_.current());
}

}
}