/**
 * @file edit_tab.cpp
 * @brief 代码编辑器标签页实现。
 * @author clk
 */

#include <ui/edit_tab.h>
#include <ui/theme.h>
#include <utils/console.h>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

namespace spiration {

edit_tab::edit_tab() {
    style.background_color = theme::editor_bg();
}

bool edit_tab::open(const std::string& path) {
    if (!doc_.load_file(path)) {
        console::warning("edit_tab: cannot open '%s'", path.c_str());
        return false;
    }
    filepath_ = path;
    title_ = path.substr(path.find_last_of("/\\") + 1);
    highlighter_.invalidate_all();
    console::info("edit_tab: opened '%s' (%zu chars, %zu lines)",
                  path.c_str(), doc_.char_count(), doc_.line_count());
    return true;
}

void edit_tab::new_file(const std::string& title) {
    doc_.set_text("");
    filepath_.clear();
    title_ = title;
    highlighter_.invalidate_all();
    cursor_line_ = cursor_col_ = 0;
    scroll_y_ = 0;
}

void edit_tab::handle_event(const event_type& type, void* data) {
    if (type == event_type::keyboard) {
        auto* kd = static_cast<key_event_data*>(data);
        kd->consumed = true;

        
        has_selection_ = false;

        size_t lc = doc_.line_count();
        if (lc == 0) { doc_.set_text("\n"); lc = 1; }
        if (cursor_line_ >= lc) cursor_line_ = lc - 1;
        size_t maxCol = doc_.line(cursor_line_).size();
        if (cursor_col_ > maxCol) cursor_col_ = maxCol;

        
        kd->ime_x = gutter_width() + 4.0f + cursor_col_ * char_width_;
        kd->ime_y = cursor_line_ * line_height_ - scroll_y_ + line_height_;

        if (kd->ctrl && kd->key_code == 'A') {
            anchor_line_ = 0; anchor_col_ = 0;
            cursor_line_ = lc - 1; cursor_col_ = maxCol; has_selection_ = true;
            if (request_repaint_) request_repaint_(); return;
        }
        if (kd->ctrl && kd->key_code == 'C') { copy_selection(); return; }
        if (kd->ctrl && kd->key_code == 'V') { paste(); return; }
        if (kd->ctrl && kd->key_code == 'X') { copy_selection(); return; }
        if (kd->ctrl && kd->key_code == 'S') {
            if (!filepath_.empty()) {
                std::ofstream out(filepath_);
                if (out.is_open()) for (size_t i = 0; i < lc; ++i) out << doc_.line(i) << '\n';
            }
            return;
        }
        if (kd->ctrl && kd->key_code == 'Z') return;

        
        if (kd->codepoint >= 32) {
            char utf8[5] = {};
            int len;
            unsigned int cp = kd->codepoint;
            if (cp < 0x80)      { utf8[0] = (char)cp; len = 1; }
            else if (cp < 0x800){ utf8[0]=(char)(0xC0|(cp>>6)); utf8[1]=(char)(0x80|(cp&0x3F)); len=2; }
            else if (cp < 0x10000){ utf8[0]=(char)(0xE0|(cp>>12)); utf8[1]=(char)(0x80|((cp>>6)&0x3F)); utf8[2]=(char)(0x80|(cp&0x3F)); len=3; }
            else { utf8[0]=(char)(0xF0|(cp>>18)); utf8[1]=(char)(0x80|((cp>>12)&0x3F)); utf8[2]=(char)(0x80|((cp>>6)&0x3F)); utf8[3]=(char)(0x80|(cp&0x3F)); len=4; }
            doc_.insert(cursor_line_, cursor_col_, std::string(utf8, len));
            cursor_col_++;
            highlighter_.invalidate_line(cursor_line_);
            if (request_repaint_) request_repaint_();
            return;
        }

        
        switch (kd->key_code) {
        case VK_RETURN: insert_char('\n'); return;
        case VK_BACK:
            if (cursor_col_ > 0) { cursor_col_--; doc_.erase(cursor_line_, cursor_col_, cursor_line_, cursor_col_ + 1); highlighter_.invalidate_line(cursor_line_); }
            else if (cursor_line_ > 0) { size_t pl = doc_.line(cursor_line_ - 1).size(); doc_.erase(cursor_line_ - 1, pl, cursor_line_, 0); cursor_line_--; cursor_col_ = pl; highlighter_.invalidate_line(cursor_line_); }
            if (request_repaint_) request_repaint_(); return;
        case VK_DELETE:
            if (cursor_col_ < maxCol) doc_.erase(cursor_line_, cursor_col_, cursor_line_, cursor_col_ + 1);
            else if (cursor_line_ + 1 < lc) doc_.erase(cursor_line_, cursor_col_, cursor_line_ + 1, 0);
            highlighter_.invalidate_line(cursor_line_);
            if (request_repaint_) request_repaint_(); return;
        case VK_LEFT:
            if (kd->shift && !has_selection_) { anchor_line_ = cursor_line_; anchor_col_ = cursor_col_; has_selection_ = true; }
            if (cursor_col_ > 0) cursor_col_--;
            else if (cursor_line_ > 0) { cursor_line_--; cursor_col_ = doc_.line(cursor_line_).size(); }
            if (!kd->shift) has_selection_ = false;
            if (request_repaint_) request_repaint_(); return;
        case VK_RIGHT:
            if (kd->shift && !has_selection_) { anchor_line_ = cursor_line_; anchor_col_ = cursor_col_; has_selection_ = true; }
            if (cursor_col_ < maxCol) cursor_col_++;
            else if (cursor_line_ + 1 < lc) { cursor_line_++; cursor_col_ = 0; }
            if (!kd->shift) has_selection_ = false;
            if (request_repaint_) request_repaint_(); return;
        case VK_UP:
            if (kd->shift && !has_selection_) { anchor_line_ = cursor_line_; anchor_col_ = cursor_col_; has_selection_ = true; }
            if (cursor_line_ > 0) cursor_line_--;
            cursor_col_ = std::min(cursor_col_, doc_.line(cursor_line_).size());
            if (!kd->shift) has_selection_ = false;
            if (request_repaint_) request_repaint_(); return;
        case VK_DOWN:
            if (kd->shift && !has_selection_) { anchor_line_ = cursor_line_; anchor_col_ = cursor_col_; has_selection_ = true; }
            if (cursor_line_ + 1 < lc) cursor_line_++;
            cursor_col_ = std::min(cursor_col_, doc_.line(cursor_line_).size());
            if (!kd->shift) has_selection_ = false;
            if (request_repaint_) request_repaint_(); return;
        case VK_HOME: cursor_col_ = 0; if (request_repaint_) request_repaint_(); return;
        case VK_END:  cursor_col_ = maxCol; if (request_repaint_) request_repaint_(); return;
        }
        return;
    }

    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        if (md->action == mouse_action::wheel) {
            float step = (md->wheel_delta > 0) ? -line_height_ * 3.0f : line_height_ * 3.0f;
            scroll_y_ += step;
            if (scroll_y_ < 0) scroll_y_ = 0;
            float maxS = static_cast<float>(doc_.line_count()) * line_height_;
            if (scroll_y_ > maxS) scroll_y_ = maxS;
            if (request_repaint_) request_repaint_(); md->consumed = true; return;
        }
        float gutter = gutter_width();
        float mx = md->position.x - gutter;
        float my = md->position.y;
        if (md->action == mouse_action::down && mx >= 0 && my >= 0) {
            has_selection_ = false;
            size_t line = static_cast<size_t>(std::max(0.0f, (my + scroll_y_) / line_height_));
            size_t col = static_cast<size_t>(std::max(0.0f, mx / char_width_));
            if (line < doc_.line_count()) {
                cursor_line_ = line; cursor_col_ = std::min(col, doc_.line(line).size());
                anchor_line_ = cursor_line_; anchor_col_ = cursor_col_;
            }
            md->consumed = true; if (request_repaint_) request_repaint_(); return;
        }
        if (md->action == mouse_action::move && (GetKeyState(VK_LBUTTON) & 0x8000) && mx >= 0) {
            if (!has_selection_) { anchor_line_ = cursor_line_; anchor_col_ = cursor_col_; has_selection_ = true; }
            size_t line = static_cast<size_t>(std::max(0.0f, (my + scroll_y_) / line_height_));
            size_t col = static_cast<size_t>(std::max(0.0f, mx / char_width_));
            if (line < doc_.line_count()) { cursor_line_ = line; cursor_col_ = std::min(col, doc_.line(line).size()); }
            md->consumed = true; if (request_repaint_) request_repaint_(); return;
        }
    }
    container::handle_event(type, data);
}

void edit_tab::insert_char(char c) {
    size_t lc = doc_.line_count(); if (lc == 0) lc = 1;
    if (cursor_line_ >= lc) cursor_line_ = lc - 1;
    if (c == '\r' || c == '\n') {
        size_t maxCol = doc_.line(cursor_line_).size();
        std::string rest = doc_.line(cursor_line_).substr(cursor_col_);
        doc_.erase(cursor_line_, cursor_col_, cursor_line_, maxCol);
        doc_.insert(cursor_line_ + 1, 0, rest);
        cursor_line_++; cursor_col_ = 0;
        highlighter_.invalidate_line(cursor_line_); highlighter_.invalidate_line(cursor_line_ - 1);
    } else if (c >= 32) {
        doc_.insert(cursor_line_, cursor_col_, std::string(1, c));
        cursor_col_++; highlighter_.invalidate_line(cursor_line_);
    }
    if (request_repaint_) request_repaint_();
}

void edit_tab::copy_selection() {
    std::string sel = get_selection_text();
    if (sel.empty()) return;
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sel.size() + 1);
        memcpy(GlobalLock(h), sel.c_str(), sel.size() + 1);
        GlobalUnlock(h); SetClipboardData(CF_TEXT, h);
        CloseClipboard();
    }
}

void edit_tab::paste() {
    if (OpenClipboard(nullptr)) {
        HANDLE h = GetClipboardData(CF_TEXT);
        if (h) { const char* txt = static_cast<const char*>(GlobalLock(h)); if (txt) { for (const char* p = txt; *p; ++p) insert_char(*p); GlobalUnlock(h); } }
        CloseClipboard();
    }
}

std::string edit_tab::get_selection_text() const {
    if (!has_selection_ || (anchor_line_ == cursor_line_ && anchor_col_ == cursor_col_)) return "";
    size_t r1 = std::min(anchor_line_, cursor_line_);
    size_t r2 = std::max(anchor_line_, cursor_line_);
    size_t c1 = (r1 == anchor_line_) ? anchor_col_ : cursor_col_;
    size_t c2 = (r2 == anchor_line_) ? anchor_col_ : cursor_col_;
    if (r1 == r2) { c1 = std::min(anchor_col_, cursor_col_); c2 = std::max(anchor_col_, cursor_col_); }
    std::string result;
    for (size_t ln = r1; ln <= r2 && ln < doc_.line_count(); ++ln) {
        if (ln == r1 && ln == r2) result += doc_.line(ln).substr(c1, c2 - c1);
        else if (ln == r1) result += doc_.line(ln).substr(c1) + "\n";
        else if (ln == r2) result += doc_.line(ln).substr(0, c2);
        else result += doc_.line(ln) + "\n";
    }
    return result;
}

void edit_tab::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({x, y, width, height}, style.background_color);

    float gutter = gutter_width();
    float firstLine = std::max(0.0f, scroll_y_ / line_height_);
    float lastLine = firstLine + height / line_height_ + 1;
    size_t totalLines = doc_.line_count();

    renderer->draw_rectangle({x, y, gutter, height}, theme::editor_gutter_bg());
    renderer->draw_line({x + gutter, y}, {x + gutter, y + height}, theme::editor_line_num(), 1.0f);

    size_t startLine = static_cast<size_t>(firstLine);
    size_t endLine = std::min(static_cast<size_t>(lastLine) + 1, totalLines);

    
    if (has_selection_ && (anchor_line_ != cursor_line_ || anchor_col_ != cursor_col_)) {
        size_t r1 = std::min(anchor_line_, cursor_line_);
        size_t r2 = std::max(anchor_line_, cursor_line_);
        for (size_t ln = r1; ln <= r2 && ln < totalLines; ++ln) {
            float ly = y + (float)ln * line_height_ - scroll_y_;
            if (ly + line_height_ < y || ly > y + height) continue;
            float sx = 0, ex = 0;
            if (ln == r1 && ln == r2) {
                size_t c1 = std::min(anchor_col_, cursor_col_), c2 = std::max(anchor_col_, cursor_col_);
                sx = (float)c1 * char_width_; ex = (float)c2 * char_width_;
            } else if (ln == r1) {
                size_t c = (r1 == anchor_line_) ? anchor_col_ : cursor_col_;
                sx = (float)c * char_width_; ex = (float)doc_.line(ln).size() * char_width_;
            } else if (ln == r2) {
                size_t c = (r2 == anchor_line_) ? anchor_col_ : cursor_col_;
                sx = 0; ex = (float)c * char_width_;
            } else {
                sx = 0; ex = (float)doc_.line(ln).size() * char_width_;
            }
            if (ex > sx) renderer->draw_rectangle({x + gutter + 4.0f + sx, ly, ex - sx, line_height_}, {0.2f, 0.5f, 0.9f, 0.3f});
        }
    }

    
    for (size_t ln = startLine; ln < endLine; ++ln) {
        float ly = y + static_cast<float>(ln) * line_height_ - scroll_y_;
        if (ly + line_height_ < y || ly > y + height) continue;
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu", ln + 1);
        renderer->draw_text_aligned(buf, {x, ly, gutter - 4.0f, line_height_}, theme::editor_line_num(),
                                    text_alignment::right, vertical_alignment::center, 12.0f);

        const auto& tokens = highlighter_.highlight_line(ln, doc_.line(ln));
        float textX = x + gutter + 4.0f;
        for (const auto& tok : tokens) {
            std::string t = doc_.line(ln).substr(tok.start, tok.len);
            renderer->draw_text(t, {textX + tok.start * char_width_, ly}, tok.col, 14.0f, "Consolas");
        }
    }

    
    if (cursor_line_ < totalLines) {
        float cx = x + gutter + 4.0f + static_cast<float>(cursor_col_) * char_width_;
        float cy = y + static_cast<float>(cursor_line_) * line_height_ - scroll_y_;
        renderer->draw_line({cx, cy}, {cx, cy + line_height_}, theme::editor_cursor(), 1.5f);
    }
}

} 
