/**
 * @file label.cpp
 * @brief 文本标签控件实现.
 * @author clk
 */

#include <ui/label.h>
#include <ui/theme_manager.h>
#include <ui/context_menu.h>
#include <ui/focus_manager.h>
#include <ui/root.h>
#include <ui/text_utils.h>
#include <application.h>
#include <extension/builtin/i18n/i18n.h>
#include <utils/clipboard.h>

#include <algorithm>

namespace spiration {

std::shared_ptr<renderer> label::current_renderer() const {
    if (cached_renderer_) return cached_renderer_;
    auto* app = application::instance();
    if (app && app->window()) return app->window()->get_renderer();
    return nullptr;
}

float label::text_width() const {
    auto r = current_renderer();
    if (!r) return 0.0f;
    const float fs = font_size > 0.0f ? font_size : 14.0f;
    return r->measure_text_width(text, fs, theme_manager::get_str(theme_manager::UI_FONT));
}

bool label::hit_text_area(float x, float y) const {
    std::vector<line_info> lines;
    layout_lines(current_renderer(), lines);
    for (const auto& ln : lines) {
        if (y >= ln.y && y <= ln.y + ln.height) {
            float lx = align_x(ln.width, width > 0.0f ? width : 10000.0f);
            return x >= lx && x <= lx + ln.width;
        }
    }
    return false;
}

float label::align_x(float line_width, float wrap_width) const {
    switch (h_align) {
        case text_alignment::center: return std::max(0.0f, (wrap_width - line_width) * 0.5f);
        case text_alignment::right:  return std::max(0.0f, wrap_width - line_width);
        default: return 0.0f;
    }
}

float label::layout_lines(std::shared_ptr<renderer> r, std::vector<line_info>& out) const {
    const float fs = font_size > 0.0f ? font_size : 14.0f;
    const std::string fam = theme_manager::get_str(theme_manager::UI_FONT);
    const float wrap_w = width > 0.0f ? width : 10000.0f;
    const float line_h = fs * 1.4f;

    if (cached_lines_valid_ && cached_lines_text_ == text &&
        cached_lines_width_ == width && cached_lines_font_ == font_size &&
        cached_lines_height_ == height) {
        out = cached_lines_;
        return cached_lines_total_h_;
    }

    out.clear();
    if (text.empty()) {
        out.push_back({0.0f, line_h, 0, 0, 0.0f, {}, {}});
        return line_h;
    }

    const size_t n = text.size();
    size_t i = 0;
    size_t line_start = 0;
    float line_w = 0.0f;
    std::vector<float> widths;
    std::vector<size_t> offsets;

    auto flush = [&]() {
        out.push_back({0.0f, line_h, line_start, i, line_w,
                       std::move(widths), std::move(offsets)});
    };

    while (i < n) {
        if (text[i] == '\n') {
            flush();
            ++i;
            line_start = i;
            line_w = 0.0f;
            widths.clear();
            offsets.clear();
            continue;
        }
        unsigned char c = static_cast<unsigned char>(text[i]);
        size_t len = 1;
        if ((c & 0xF0) == 0xF0) len = 4;
        else if ((c & 0xE0) == 0xE0) len = 3;
        else if ((c & 0xC0) == 0xC0) len = 2;
        if (i + len > n) len = n - i;
        std::string ch = text.substr(i, len);
        float chw = r ? r->measure_text_width(ch, fs, fam) : fs * 0.5f;

        if (!widths.empty() && line_w + chw > wrap_w) {
            flush();
            line_start = i;
            line_w = 0.0f;
            widths.clear();
            offsets.clear();
        }
        offsets.push_back(i);
        widths.push_back(chw);
        line_w += chw;
        i += len;
    }
    if (!widths.empty() || out.empty()) flush();

    float total_h = static_cast<float>(out.size()) * line_h;
    float start_y = 0.0f;
    if (v_align == vertical_alignment::center)
        start_y = std::max(0.0f, (height - total_h) * 0.5f);
    else if (v_align == vertical_alignment::bottom)
        start_y = std::max(0.0f, height - total_h);
    float y = start_y;
    for (auto& ln : out) {
        ln.y = y;
        ln.height = line_h;
        y += line_h;
    }

    cached_lines_ = out;
    cached_lines_total_h_ = total_h;
    cached_lines_text_ = text;
    cached_lines_width_ = width;
    cached_lines_font_ = font_size;
    cached_lines_height_ = height;
    cached_lines_valid_ = true;
    return total_h;
}

void label::paint(std::shared_ptr<renderer> renderer) {
    cached_renderer_ = renderer;

    if (!selectable) {
        std::string display = text;
        if (overflow != text_overflow::none && !display.empty()) {
            float avail = std::max(0.0f, width);
            float tw = renderer->measure_text_width(
                display, font_size > 0.0f ? font_size : 14.0f,
                theme_manager::get_str(theme_manager::UI_FONT));
            if (tw > avail) {
                if (overflow == text_overflow::hide) {
                    display.clear();
                } else {
                    display = text_utils::ellipsize(renderer, display, font_size, avail);
                }
            }
        }
        renderer->draw_text_aligned(
            display,
            {0, 0, width, height},
            theme_manager::get(theme_manager::LABEL_TEXT),
            h_align,
            v_align,
            font_size);
        return;
    }

    std::vector<line_info> lines;
    layout_lines(renderer, lines);
    const float fs = font_size > 0.0f ? font_size : 14.0f;
    const std::string fam = theme_manager::get_str(theme_manager::UI_FONT);
    const color c = theme_manager::get(theme_manager::LABEL_TEXT);
    const float wrap_w = width > 0.0f ? width : 10000.0f;
    for (const auto& ln : lines) {
        if (ln.start >= ln.end) continue;
        float lx = align_x(ln.width, wrap_w);
        renderer->draw_text(text.substr(ln.start, ln.end - ln.start), {lx, ln.y},
                            c, fs, fam, false);
    }
    draw_selection_highlight(renderer);
}

void label::draw_selection_highlight(std::shared_ptr<renderer> r) const {
    if (!has_selection()) return;
    size_t a = std::min(sel_anchor_, sel_pos_);
    size_t b = std::max(sel_anchor_, sel_pos_);
    std::vector<line_info> lines;
    layout_lines(r, lines);
    const float wrap_w = width > 0.0f ? width : 10000.0f;
    const color sel_c = {0.20f, 0.40f, 0.80f, 0.35f};

    for (const auto& ln : lines) {
        if (ln.end <= a || ln.start >= b) continue;
        float xa = 0.0f, xb = ln.width;
        float acc = 0.0f;
        bool started = false;
        for (size_t k = 0; k < ln.widths.size(); ++k) {
            size_t coff = ln.char_offsets[k];
            size_t cend = (k + 1 < ln.widths.size()) ? ln.char_offsets[k + 1] : ln.end;
            if (!started && cend > a) { xa = acc; started = true; }
            if (started && coff >= b) { xb = acc; break; }
            acc += ln.widths[k];
        }
        float lx = align_x(ln.width, wrap_w);
        r->draw_rectangle({lx + xa, ln.y, std::max(xb - xa, 1.0f), ln.height}, sel_c);
    }
}

size_t label::hit_test_text(float x, float y) const {
    std::vector<line_info> lines;
    layout_lines(current_renderer(), lines);
    if (lines.empty()) return 0;
    const float wrap_w = width > 0.0f ? width : 10000.0f;

    size_t idx = lines.size();
    for (size_t i = 0; i < lines.size(); ++i) {
        if (y <= lines[i].y + lines[i].height + 1.0f) {
            idx = i;
            break;
        }
    }
    if (idx == lines.size()) {
        return text.size();
    }
    const auto& ln = lines[idx];
    if (y < ln.y - 1.0f) {
        return ln.start;
    }
    float lx = align_x(ln.width, wrap_w);
    float rel = x - lx;
    float acc = 0.0f;
    for (size_t k = 0; k < ln.widths.size(); ++k) {
        if (rel <= acc + ln.widths[k] * 0.5f)
            return ln.char_offsets[k];
        acc += ln.widths[k];
    }
    return ln.end;
}

std::string label::selected_text() const {
    if (!has_selection()) return {};
    size_t a = std::min(sel_anchor_, sel_pos_);
    size_t b = std::max(sel_anchor_, sel_pos_);
    if (b > text.size()) b = text.size();
    if (a >= b) return {};
    return text.substr(a, b - a);
}

void label::clear_selection() {
    selecting_ = false;
    sel_anchor_ = 0;
    sel_pos_ = 0;
    if (request_repaint_) request_repaint_();
}

void label::notify_root_selection_started() {
    for (widget* p = parent(); p; p = p->parent()) {
        if (auto* r = dynamic_cast<root*>(p)) {
            r->notify_selection_started(this);
            break;
        }
    }
}

void label::open_context_menu(float mx, float my) {
    auto menu = std::make_unique<context_menu>();
    menu->add_item(i18n_manager::get().tr("context.copy"), [this]() {
        if (has_selection()) clipboard::copy(selected_text());
    });
    menu->add_item(i18n_manager::get().tr("context.select_all"), [this]() {
        notify_root_selection_started();
        sel_anchor_ = 0;
        sel_pos_ = text.size();
        selecting_ = true;
        if (request_repaint_) request_repaint_();
    });
    point sp = to_screen(mx, my);
    request_context_menu(sp.x, sp.y, std::move(menu));
}

void label::handle_event(const event_type& type, void* data) {
    if (selectable) {
        if (type == event_type::mouse) {
            auto* md = static_cast<mouse_event_data*>(data);
            const float mx = md->position.x;
            const float my = md->position.y;
            const bool inside = hit_text_area(mx, my);

            if (md->action == mouse_action::down && md->button == mouse_button::left && inside) {
                notify_root_selection_started();
                md->consumed = true;
                focus_manager::instance().clear_focus();
                mouse_down_ = true;
                selecting_ = true;
                sel_anchor_ = hit_test_text(mx, my);
                sel_pos_ = sel_anchor_;
                set_mouse_capture(this);
                if (request_repaint_) request_repaint_();
            } else if (md->action == mouse_action::down && md->button == mouse_button::right && inside) {
                md->consumed = true;
                open_context_menu(mx, my);
            } else if (md->action == mouse_action::move && mouse_down_) {
                sel_pos_ = hit_test_text(mx, my);
                if (request_repaint_) request_repaint_();
            } else if (md->action == mouse_action::up && mouse_down_) {
                mouse_down_ = false;
                sel_pos_ = hit_test_text(mx, my);
                set_mouse_capture(nullptr);
                if (request_repaint_) request_repaint_();
            }
        } else if (type == event_type::keyboard) {
            auto* kd = static_cast<key_event_data*>(data);
            if (kd->ctrl && !kd->shift) {
                if (kd->key_code == 67 && has_selection()) {  // Ctrl+C
                    clipboard::copy(selected_text());
                    kd->consumed = true;
                } else if (kd->key_code == 65) {              // Ctrl+A
                    notify_root_selection_started();
                    sel_anchor_ = 0;
                    sel_pos_ = text.size();
                    selecting_ = true;
                    kd->consumed = true;
                    if (request_repaint_) request_repaint_();
                }
            }
        }
    }
    widget::handle_event(type, data);
}

size label::layout_preferred_size() const {
    float w = width;

    if (selectable) {
        std::vector<line_info> lines;
        float h = layout_lines(current_renderer(), lines);
        if (w <= 0.0f) w = font_size * 4.0f;
        return {w, h};
    }

    if (!text.empty()) {
        auto r = cached_renderer_;
        if (!r) r = current_renderer();
        if (r) {
            float measured_w = r->measure_text_width(text, font_size,
                theme_manager::get_str(theme_manager::UI_FONT));
            float measured_h = r->measure_text_height(text, font_size,
                theme_manager::get_str(theme_manager::UI_FONT),
                w > 0.0f ? w : 10000.0f);
            if (w <= 0.0f) w = measured_w;
            return {w > 0.0f ? w : font_size * 4.0f,
                    measured_h > 0.0f ? measured_h : font_size * 1.4f};
        }
    }

    if (w <= 0.0f) w = font_size * 4.0f;
    int lines = 1;
    for (size_t pos = 0; (pos = text.find('\n', pos)) != std::string::npos; ++pos)
        ++lines;
    return {w, font_size * 1.4f * lines};
}

void label::layout() {
    on_layout_begin();
    if (selectable) {
        std::vector<line_info> lines;
        float h = layout_lines(current_renderer(), lines);
        if (h > 0.0f) height = h;
        return;
    }
    size pref = layout_preferred_size();
    if (pref.height > 0.0f) height = pref.height;
}

} // namespace spiration

