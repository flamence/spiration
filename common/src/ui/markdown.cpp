/**
 * @file markdown.cpp
 * @brief Markdown 文本控件实现。
 * @author clk
 */

#include <ui/markdown.h>
#include <ui/theme_manager.h>
#include <application.h>
#include <extension/builtin/i18n/i18n.h>
#include <utils/platform.h>

#include <algorithm>
#include <cstdlib>
#include <map>

namespace spiration {

namespace {

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

std::vector<std::pair<std::string, size_t>> split_lines_off(const std::string& src) {
    std::vector<std::pair<std::string, size_t>> out;
    std::string cur;
    size_t line_start = 0;
    for (size_t i = 0; i <= src.size(); ++i) {
        if (i == src.size() || src[i] == '\n') {
            out.push_back({cur, line_start});
            cur.clear();
            line_start = i + 1;
        } else {
            cur += src[i];
        }
    }
    return out;
}

std::vector<std::pair<std::string, size_t>> split_newlines_off(const std::string& s) {
    std::vector<std::pair<std::string, size_t>> out;
    std::string cur;
    size_t seg_start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == '\n') {
            out.push_back({cur, seg_start});
            cur.clear();
            seg_start = i + 1;
        } else {
            cur += s[i];
        }
    }
    return out;
}

std::vector<std::pair<std::string, size_t>> split_words_off(const std::string& s) {
    std::vector<std::pair<std::string, size_t>> out;
    std::string cur;
    size_t word_start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        bool delim = (i == s.size() || s[i] == ' ' || s[i] == '\t');
        if (delim) {
            if (!cur.empty()) { out.push_back({cur, word_start}); cur.clear(); }
        } else {
            if (cur.empty()) word_start = i;
            cur += s[i];
        }
    }
    return out;
}

bool is_hr(const std::string& t) {
    if (t.size() < 3) return false;
    char c = t[0];
    if (c != '-' && c != '*' && c != '_') return false;
    size_t count = 0;
    for (char ch : t) {
        if (ch == c) ++count;
        else if (ch != ' ' && ch != '\t') return false;
    }
    return count >= 3;
}

bool is_list_marker(const std::string& t, bool& ordered, int& number) {
    if (t.size() >= 2) {
        char c = t[0];
        if ((c == '-' || c == '*' || c == '+') &&
            (t[1] == ' ' || t[1] == '\t')) {
            ordered = false;
            number = 1;
            return true;
        }
    }
    size_t j = 0;
    while (j < t.size() && t[j] >= '0' && t[j] <= '9') ++j;
    if (j > 0 && j < t.size() && t[j] == '.' &&
        j + 1 < t.size() && (t[j + 1] == ' ' || t[j + 1] == '\t')) {
        ordered = true;
        number = std::atoi(t.substr(0, j).c_str());
        if (number <= 0) number = 1;
        return true;
    }
    return false;
}

std::string strip_list_marker(const std::string& t) {
    if (t.size() >= 2 && (t[0] == '-' || t[0] == '*' || t[0] == '+'))
        return trim(t.substr(1));
    size_t j = 0;
    while (j < t.size() && t[j] >= '0' && t[j] <= '9') ++j;
    if (j > 0 && j < t.size() && t[j] == '.') return trim(t.substr(j + 1));
    return t;
}

float heading_size(float base, int level) {
    switch (level) {
        case 1: return base * 1.55f;
        case 2: return base * 1.40f;
        case 3: return base * 1.25f;
        case 4: return base * 1.12f;
        default: return base;
    }
}

std::string ui_font() {
    return theme_manager::get_str(theme_manager::UI_FONT);
}

std::string mono_font() {
    return theme_manager::get_str(theme_manager::EDITOR_FONT, "Consolas");
}

std::string family_for(markdown::run_style s, const std::string& base) {
    return s == markdown::run_style::code ? mono_font() : base;
}

float approx_width(const std::string& s, float font_size) {
    return static_cast<float>(s.size()) * font_size * 0.5f;
}

std::vector<std::vector<markdown::run>> wrap_words(
        const std::vector<markdown::run>& runs, float width, float font_size,
        std::shared_ptr<renderer> r, const std::string& base_family) {
    std::vector<std::vector<markdown::run>> lines;
    std::vector<markdown::run> line;
    float line_w = 0.0f;
    float space_w = r ? r->measure_text_width(" ", font_size, base_family)
                      : font_size * 0.28f;

    auto flush = [&]() {
        if (!line.empty()) {
            lines.push_back(std::move(line));
            line.clear();
        }
        line_w = 0.0f;
    };

    for (const auto& run : runs) {
        if (run.style == markdown::run_style::image) {
            std::string family = family_for(run.style, base_family);
            float ww = r ? r->measure_text_width(run.text, font_size, family)
                         : approx_width(run.text, font_size);
            ww = std::max(28.0f, ww + 14.0f);
            if (!line.empty() && line_w + space_w + ww > width) flush();
            line_w += (line.empty() ? 0.0f : space_w) + ww;
            line.push_back(run);
            continue;
        }
        std::string family = family_for(run.style, base_family);
        auto segs = split_newlines_off(run.text);
        for (size_t s = 0; s < segs.size(); ++s) {
            if (s > 0) flush();
            auto words = split_words_off(segs[s].first);
            for (const auto& wp : words) {
                float ww = r ? r->measure_text_width(wp.first, font_size, family)
                             : approx_width(wp.first, font_size);
                if (!line.empty() && line_w + space_w + ww > width) flush();
                line_w += (line.empty() ? 0.0f : space_w) + ww;
                markdown::run wr;
                wr.text = wp.first;
                wr.style = run.style;
                wr.href = run.href;
                wr.src = run.src + segs[s].second + wp.second;
                line.push_back(std::move(wr));
            }
        }
    }
    flush();
    return lines;
}

float line_width(std::shared_ptr<renderer> r, const std::vector<markdown::run>& line,
                 float font_size, const std::string& base_family) {
    float w = 0.0f;
    float space_w = r ? r->measure_text_width(" ", font_size, base_family)
                      : font_size * 0.28f;
    for (size_t k = 0; k < line.size(); ++k) {
        const auto& run = line[k];
        std::string family = family_for(run.style, base_family);
        w += r ? r->measure_text_width(run.text, font_size, family)
               : approx_width(run.text, font_size);
        if (k + 1 < line.size()) w += space_w;
    }
    return w;
}

} // namespace

markdown::markdown(const std::string& text) {
    this->text = text;
}

void markdown::ensure_parsed() const {
    if (parsed_text_ != text) {
        blocks_ = parse(text);
        parsed_text_ = text;
    }
}

std::shared_ptr<renderer> markdown::measure_renderer() const {
    if (cached_renderer_) return cached_renderer_;
    auto* app = application::instance();
    if (app && app->window()) return app->window()->get_renderer();
    return nullptr;
}

float markdown::effective_width() const {
    if (width > 0.0f) return width;
    auto r = measure_renderer();
    uint32_t vw = 0, vh = 0;
    if (r) {
        r->get_viewport_size(vw, vh);
        if (vw > 0) return static_cast<float>(vw);
    }
    return 480.0f;
}

std::vector<markdown::block> markdown::parse(const std::string& src) const {
    std::vector<block> blocks;
    auto src_lines = split_lines_off(src);
    const size_t n = src_lines.size();
    size_t i = 0;

    std::map<std::string, std::string> refs;
    for (const auto& [line, loff] : src_lines) {
        std::string t = trim(line);
        if (t.size() >= 4 && t[0] == '[') {
            size_t close = t.find(']');
            if (close != std::string::npos && close + 2 < t.size() && t[close + 1] == ':') {
                std::string id = t.substr(1, close - 1);
                std::string rest = trim(t.substr(close + 2));
                std::string url;
                if (!rest.empty() && rest[0] == '<') {
                    size_t gt = rest.find('>');
                    if (gt != std::string::npos) url = rest.substr(1, gt - 1);
                } else {
                    size_t sp = rest.find_first_of(" \t");
                    url = (sp == std::string::npos) ? rest : rest.substr(0, sp);
                }
                if (!id.empty() && !url.empty()) refs[id] = url;
            }
        }
    }

    while (i < n) {
        const std::string& raw = src_lines[i].first;
        const size_t loff = src_lines[i].second;
        size_t lead = raw.find_first_not_of(" \t\r");
        if (lead == std::string::npos) lead = 0;
        const size_t t_off = loff + lead;
        std::string t = trim(raw);
        if (t.empty()) { ++i; continue; }

        if (t.rfind("```", 0) == 0) {
            block b;
            b.k = block::kind::code;
            ++i;
            while (i < n) {
                const std::string& cl = src_lines[i].first;
                if (trim(cl).rfind("```", 0) == 0) { ++i; break; }
                b.code_lines.push_back(cl);
                ++i;
            }
            blocks.push_back(std::move(b));
            continue;
        }

        if (t[0] == '#') {
            size_t j = 1;
            while (j < t.size() && t[j] == '#') ++j;
            block b;
            b.k = block::kind::heading;
            b.level = static_cast<int>(std::min<size_t>(j, 6));
            if (j < t.size() && t[j] == ' ') ++j;
            b.runs = parse_inline(t.substr(j), t_off + j, refs);
            blocks.push_back(std::move(b));
            ++i;
            continue;
        }

        if (is_hr(t)) {
            block b;
            b.k = block::kind::hr;
            blocks.push_back(std::move(b));
            ++i;
            continue;
        }

        if (t.rfind(">", 0) == 0) {
            block b;
            b.k = block::kind::quote;
            while (i < n) {
                const std::string& qraw = src_lines[i].first;
                const size_t qoff = src_lines[i].second;
                size_t qlead = qraw.find_first_not_of(" \t\r");
                if (qlead == std::string::npos) qlead = 0;
                std::string qt = trim(qraw);
                if (qt.rfind(">", 0) != 0) break;
                std::string content = qt.substr(1);
                size_t content_off = qoff + qlead + 1;
                if (!content.empty() && content[0] == ' ') { content.erase(0, 1); ++content_off; }
                if (!b.runs.empty()) b.runs.push_back({"\n", run_style::normal, "", 0});
                auto runs = parse_inline(content, content_off, refs);
                b.runs.insert(b.runs.end(), runs.begin(), runs.end());
                ++i;
            }
            blocks.push_back(std::move(b));
            continue;
        }

        bool ordered = false;
        int number = 0;
        if (is_list_marker(t, ordered, number)) {
            block b;
            b.k = block::kind::list;
            b.ordered = ordered;
            b.level = number;
            while (i < n) {
                const std::string& lraw = src_lines[i].first;
                const size_t loff2 = src_lines[i].second;
                size_t llead = lraw.find_first_not_of(" \t\r");
                if (llead == std::string::npos) llead = 0;
                std::string lt = trim(lraw);
                if (lt.empty()) break;
                bool o2 = false;
                int n2 = 0;
                if (!is_list_marker(lt, o2, n2)) break;
                size_t mark_end = 2;  // "- " / "* " / "+ "
                if (lt[0] >= '0' && lt[0] <= '9') {
                    size_t jj = 0;
                    while (jj < lt.size() && lt[jj] >= '0' && lt[jj] <= '9') ++jj;
                    mark_end = jj + 2;  // "N. "
                }
                while (mark_end < lt.size() && (lt[mark_end] == ' ' || lt[mark_end] == '\t'))
                    ++mark_end;
                b.items.push_back(parse_inline(strip_list_marker(lt), loff2 + llead + mark_end, refs));
                ++i;
            }
            blocks.push_back(std::move(b));
            continue;
        }

        block b;
        b.k = block::kind::paragraph;
        while (i < n) {
            const std::string& praw = src_lines[i].first;
            const size_t poff = src_lines[i].second;
            size_t plead = praw.find_first_not_of(" \t\r");
            if (plead == std::string::npos) plead = 0;
            std::string pt = trim(praw);
            if (pt.empty()) { ++i; break; }
            bool o2 = false;
            int n2 = 0;
            if (pt.rfind("```", 0) == 0 || pt[0] == '#' || is_hr(pt) ||
                pt.rfind(">", 0) == 0 || is_list_marker(pt, o2, n2))
                break;
            if (!b.runs.empty()) b.runs.push_back({"\n", run_style::normal, "", 0});
            auto runs = parse_inline(pt, poff + plead, refs);
            b.runs.insert(b.runs.end(), runs.begin(), runs.end());
            ++i;
        }
        blocks.push_back(std::move(b));
    }
    return blocks;
}

std::vector<markdown::run> markdown::parse_inline(
        const std::string& line, size_t line_off,
        const std::map<std::string, std::string>& refs) {
    std::vector<run> out;
    std::string cur;
    size_t cur_start = 0;
    auto flush = [&]() {
        if (!cur.empty()) {
            run r;
            r.text = cur;
            r.src = line_off + cur_start;
            out.push_back(std::move(r));
            cur.clear();
        }
    };
    const size_t n = line.size();
    size_t i = 0;
    while (i < n) {
        char c = line[i];

        if (c == '\\' && i + 1 < n) {
            if (cur.empty()) cur_start = i;
            cur += line[i + 1];
            i += 2;
            continue;
        }
        if (c == '`') {
            size_t end = line.find('`', i + 1);
            if (end != std::string::npos) {
                flush();
                run r;
                r.text = line.substr(i + 1, end - i - 1);
                r.style = run_style::code;
                r.src = line_off + i + 1;
                out.push_back(std::move(r));
                i = end + 1;
                continue;
            }
        }
        if (c == '*' && i + 1 < n && line[i + 1] == '*') {
            size_t end = line.find("**", i + 2);
            if (end != std::string::npos) {
                flush();
                run r;
                r.text = line.substr(i + 2, end - i - 2);
                r.style = run_style::bold;
                r.src = line_off + i + 2;
                out.push_back(std::move(r));
                i = end + 2;
                continue;
            }
        }
        if (c == '*') {
            size_t end = line.find('*', i + 1);
            if (end != std::string::npos && (end + 1 >= n || line[end + 1] != '*')) {
                flush();
                run r;
                r.text = line.substr(i + 1, end - i - 1);
                r.style = run_style::italic;
                r.src = line_off + i + 1;
                out.push_back(std::move(r));
                i = end + 1;
                continue;
            }
        }
        if (c == '!' && i + 1 < n && line[i + 1] == '[') {
            size_t close = line.find(']', i + 2);
            if (close != std::string::npos && close + 1 < n && line[close + 1] == '(') {
                size_t url_end = line.find(')', close + 2);
                if (url_end != std::string::npos) {
                    flush();
                    run r;
                    r.text = line.substr(i + 2, close - i - 2);
                    r.style = run_style::image;
                    r.href = trim(line.substr(close + 2, url_end - close - 2));
                    r.src = line_off + i + 2;
                    out.push_back(std::move(r));
                    i = url_end + 1;
                    continue;
                }
            }
        }
        if (c == '<') {
            bool http = line.compare(i, 7, "http://") == 0;
            bool https = !http && line.compare(i, 8, "https://") == 0;
            if (http || https) {
                size_t end = line.find('>', i + 1);
                if (end != std::string::npos) {
                    flush();
                    run r;
                    r.text = line.substr(i + 1, end - i - 1);
                    r.style = run_style::link;
                    r.href = r.text;
                    r.src = line_off + i + 1;
                    out.push_back(std::move(r));
                    i = end + 1;
                    continue;
                }
            }
        }
        if (c == '[') {
            size_t close = line.find(']', i + 1);
            if (close != std::string::npos && close + 1 < n && line[close + 1] == '(') {
                size_t url_end = line.find(')', close + 2);
                if (url_end != std::string::npos) {
                    flush();
                    run r;
                    r.text = line.substr(i + 1, close - i - 1);
                    r.style = run_style::link;
                    r.href = trim(line.substr(close + 2, url_end - close - 2));
                    r.src = line_off + i + 1;
                    out.push_back(std::move(r));
                    i = url_end + 1;
                    continue;
                }
            }
            if (close != std::string::npos) {
                std::string label = line.substr(i + 1, close - i - 1);
                std::string id;
                size_t end_pos = close;
                bool have_ref = false;
                if (close + 1 < n && line[close + 1] == '[') {
                    size_t id_end = line.find(']', close + 2);
                    if (id_end != std::string::npos) {
                        id = line.substr(close + 2, id_end - close - 2);
                        have_ref = true;
                        end_pos = id_end;
                    }
                }
                if (have_ref && id.empty()) id = label;
                std::string href;
                if (have_ref) {
                    auto it = refs.find(id);
                    if (it != refs.end()) href = it->second;
                } else {
                    auto it = refs.find(label);
                    if (it != refs.end()) href = it->second;
                }
                if (!href.empty()) {
                    flush();
                    run r;
                    r.text = label;
                    r.style = run_style::link;
                    r.href = href;
                    r.src = line_off + i + 1;
                    out.push_back(std::move(r));
                    i = end_pos + 1;
                    continue;
                }
            }
        }

        if (cur.empty()) cur_start = i;
        cur += c;
        ++i;
    }
    flush();
    return out;
}

float markdown::measure_height(std::shared_ptr<renderer> r, float avail_w) const {
    ensure_parsed();
    const float base_size = font_size > 0.0f ? font_size : 14.0f;
    const float line_h = base_size * 1.4f;
    std::string bf = ui_font();

    float y = 0.0f;
    for (const auto& b : blocks_) {
        switch (b.k) {
            case block::kind::heading: {
                float hs = heading_size(base_size, b.level);
                float hh = hs * 1.4f;
                auto lines = wrap_words(b.runs, avail_w, hs, r, bf);
                y += hh * 0.5f + static_cast<float>(lines.size()) * hh + hh * 0.35f;
                break;
            }
            case block::kind::paragraph: {
                auto lines = wrap_words(b.runs, avail_w, base_size, r, bf);
                y += static_cast<float>(lines.size()) * line_h + base_size * 0.35f;
                break;
            }
            case block::kind::code: {
                const float pad = 6.0f;
                size_t rows = std::max<size_t>(b.code_lines.size(), 1);
                y += pad * 2.0f + static_cast<float>(rows) * line_h + base_size * 0.35f;
                break;
            }
            case block::kind::quote: {
                const float indent = 12.0f;
                auto lines = wrap_words(b.runs, avail_w - indent, base_size, r, bf);
                y += static_cast<float>(lines.size()) * line_h + base_size * 0.35f;
                break;
            }
            case block::kind::list: {
                float marker_w = b.ordered
                    ? (r ? r->measure_text_width("88. ", base_size, bf) : base_size * 2.5f)
                    : (r ? r->measure_text_width("\xE2\x80\xA2 ", base_size, bf) : base_size * 1.2f);
                float content_w = avail_w - marker_w;
                float h = 0.0f;
                for (const auto& item : b.items) {
                    auto lines = wrap_words(item, content_w, base_size, r, bf);
                    h += static_cast<float>(lines.size()) * line_h + base_size * 0.15f;
                }
                y += h + base_size * 0.25f;
                break;
            }
            case block::kind::hr:
                y += line_h * 1.3f;
                break;
        }
    }
    return y;
}

void markdown::layout() {
    auto r = measure_renderer();
    float h = measure_height(r, effective_width());
    if (h > 0.0f) height = h;
}

size markdown::layout_preferred_size() const {
    auto r = measure_renderer();
    float h = measure_height(r, effective_width());
    return {effective_width(), h};
}

void markdown::draw_block(std::shared_ptr<renderer> r, const block& b,
                          float x, float y, float avail_w, float& y_out) const {
    const float base_size = font_size > 0.0f ? font_size : 14.0f;
    const float line_h = base_size * 1.4f;
    std::string bf = ui_font();
    color base    = theme_manager::get(theme_manager::LABEL_TEXT);
    color heading = theme_manager::get(theme_manager::HEADING_TEXT);
    color code_bg = theme_manager::get(theme_manager::CODE_BG);
    color code_col = theme_manager::get(theme_manager::CODE_TEXT);
    color link    = theme_manager::get(theme_manager::LINK_TEXT);
    color muted   = theme_manager::get(theme_manager::TEXT_MUTED);
    color bar     = theme_manager::get(theme_manager::QUOTE_BAR);

    switch (b.k) {
        case block::kind::heading: {
            float hs = heading_size(base_size, b.level);
            float hh = hs * 1.4f;
            y += hh * 0.5f;
            auto lines = wrap_words(b.runs, avail_w, hs, r, bf);
            for (const auto& l : lines) {
                draw_word_line(r, l, x, y, hs, bf, heading, code_bg, hh, link);
                y += hh;
            }
            y += hh * 0.35f;
            break;
        }
        case block::kind::paragraph: {
            auto lines = wrap_words(b.runs, avail_w, base_size, r, bf);
            for (const auto& l : lines) {
                float lw = line_width(r, l, base_size, bf);
                float lx = x;
                if (h_align == text_alignment::center)
                    lx = x + std::max(0.0f, (avail_w - lw) * 0.5f);
                else if (h_align == text_alignment::right)
                    lx = x + std::max(0.0f, avail_w - lw);
                draw_word_line(r, l, lx, y, base_size, bf, base, code_bg, line_h, link);
                y += line_h;
            }
            y += base_size * 0.35f;
            break;
        }
        case block::kind::code: {
            const float pad = 6.0f;
            size_t rows = std::max<size_t>(b.code_lines.size(), 1);
            float ch = pad * 2.0f + static_cast<float>(rows) * line_h;
            r->draw_rectangle({x, y, avail_w, ch}, code_bg);
            r->draw_rectangle_outline({x, y, avail_w, ch},
                                      theme_manager::get(theme_manager::SEPARATOR), 1.0f);
            float cy = y + pad;
            for (const auto& cl : b.code_lines) {
                r->draw_text(cl, {x + pad, cy}, code_col, base_size * 0.95f, mono_font(), false);
                cy += line_h;
            }
            y += ch + base_size * 0.35f;
            break;
        }
        case block::kind::quote: {
            const float indent = 12.0f;
            auto lines = wrap_words(b.runs, avail_w - indent, base_size, r, bf);
            float qh = static_cast<float>(lines.size()) * line_h;
            r->draw_rectangle({x, y, 3.0f, qh}, bar);
            for (const auto& l : lines) {
                draw_word_line(r, l, x + indent, y, base_size, bf, muted, code_bg, line_h, link);
                y += line_h;
            }
            y += base_size * 0.35f;
            break;
        }
        case block::kind::list: {
            std::string probe = b.ordered ? "88. " : "\xE2\x80\xA2 ";
            float marker_w = r ? r->measure_text_width(probe, base_size, bf)
                               : (b.ordered ? base_size * 2.5f : base_size * 1.2f);
            float content_w = avail_w - marker_w;
            int num = b.level;
            for (const auto& item : b.items) {
                std::string marker = b.ordered ? (std::to_string(num) + ". ") : "\xE2\x80\xA2 ";
                float mw = r ? r->measure_text_width(marker, base_size, bf) : marker_w;
                auto lines = wrap_words(item, content_w, base_size, r, bf);
                r->draw_text(marker, {x, y}, base, base_size, bf, false);
                for (const auto& l : lines) {
                    draw_word_line(r, l, x + mw, y, base_size, bf, base, code_bg, line_h, link);
                    y += line_h;
                }
                y += base_size * 0.15f;
                if (b.ordered) ++num;
            }
            y += base_size * 0.25f;
            break;
        }
        case block::kind::hr: {
            r->draw_rectangle({x, y + line_h * 0.5f, avail_w, 1.0f},
                              theme_manager::get(theme_manager::SEPARATOR));
            y += line_h * 1.3f;
            break;
        }
    }
    y_out = y;
}

void markdown::draw_word_line(std::shared_ptr<renderer> r, const std::vector<run>& line,
                              float x, float y, float font_size, const std::string& base_family,
                              const color& base_color, const color& code_bg, float line_height,
                              const color& link_color) const {
    float cx = x;
    float space_w = r ? r->measure_text_width(" ", font_size, base_family)
                      : font_size * 0.28f;
    for (const auto& run : line) {
        if (run.style == run_style::image) {
            float bw = draw_image_run(r, run, cx, y, font_size, line_height);
            word_rects_.push_back({cx, y, bw, line_height, run.src,
                                   run.src + run.text.size(), run.href,
                                   run.text, run.style, font_size});
            cx += bw + space_w;
            continue;
        }
        std::string family = family_for(run.style, base_family);
        float w = r ? r->measure_text_width(run.text, font_size, family)
                    : approx_width(run.text, font_size);
        color c = base_color;
        if (run.style == run_style::link) c = link_color;

        if (run.style == run_style::code && code_bg.a > 0.0f)
            r->draw_rectangle({cx - 3.0f, y - 1.0f, w + 6.0f, line_height - 1.0f}, code_bg);

        if (run.style == run_style::bold || run.style == run_style::bold_italic)
            r->draw_text(run.text, {cx + 0.5f, y}, c, font_size, family, false);
        r->draw_text(run.text, {cx, y}, c, font_size, family, false);

        if (run.style == run_style::link)
            r->draw_line({cx, y + font_size + 1.0f},
                         {cx + w, y + font_size + 1.0f}, link_color, 1.0f);

        word_rects_.push_back({cx, y, w + space_w, line_height, run.src,
                               run.src + run.text.size(), run.href,
                               run.text, run.style, font_size});
        cx += w + space_w;
    }
}

float markdown::draw_image_run(std::shared_ptr<renderer> r, const run& img, float x, float y,
                               float font_size, float line_height) const {
    std::string path = img.href;
    if (path.rfind("file://", 0) == 0) path = path.substr(7);
    uint32_t iw = 0, ih = 0;
    bool has_img = !path.empty() && platform::file_exists(path) &&
                   r && r->query_image_size(path, iw, ih) && iw > 0 && ih > 0;
    if (has_img) {
        float dh = line_height;
        float dw = dh * static_cast<float>(iw) / static_cast<float>(ih);
        if (dw > 120.0f) {
            dw = 120.0f;
            dh = dw * static_cast<float>(ih) / static_cast<float>(iw);
        }
        r->draw_image(path, {x, y, dw, dh});
        r->draw_rectangle_outline({x, y, dw, dh},
                                  theme_manager::get(theme_manager::SEPARATOR), 1.0f);
        return dw;
    }

    std::string label = i18n_manager::get().tr("markdown.image");
    if (!img.text.empty()) label += " " + img.text;
    float lw = r ? r->measure_text_width(label, font_size, mono_font())
                 : approx_width(label, font_size);
    float bw = std::max(28.0f, lw + 14.0f);
    color border = theme_manager::get(theme_manager::SEPARATOR);
    color bg = theme_manager::get(theme_manager::CODE_BG);
    r->draw_rectangle({x, y, bw, line_height}, bg);
    r->draw_rectangle_outline({x, y, bw, line_height}, border, 1.0f);
    r->draw_text(label, {x + 7.0f, y + (line_height - font_size) * 0.5f},
                 theme_manager::get(theme_manager::TEXT_MUTED), font_size, mono_font(), false);
    return bw;
}

size_t markdown::word_offset_at(const word_rect& wr, float rel_x, std::shared_ptr<renderer> r) {
    std::string family = family_for(wr.style, ui_font());
    float acc = 0.0f;
    size_t i = 0;
    const size_t n = wr.text.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(wr.text[i]);
        size_t len = 1;
        if ((c & 0xF0) == 0xF0) len = 4;
        else if ((c & 0xE0) == 0xE0) len = 3;
        else if ((c & 0xC0) == 0xC0) len = 2;
        if (i + len > n) len = n - i;
        float cw = r ? r->measure_text_width(wr.text.substr(i, len), wr.font_size, family)
                     : wr.font_size * 0.5f;
        if (rel_x <= acc + cw * 0.5f) return i;
        acc += cw;
        i += len;
    }
    return n;
}

float markdown::word_prefix_width(const word_rect& wr, size_t byte_len, std::shared_ptr<renderer> r) {
    std::string family = family_for(wr.style, ui_font());
    float w = 0.0f;
    size_t i = 0;
    const size_t n = wr.text.size();
    if (byte_len > n) byte_len = n;
    while (i < byte_len) {
        unsigned char c = static_cast<unsigned char>(wr.text[i]);
        size_t len = 1;
        if ((c & 0xF0) == 0xF0) len = 4;
        else if ((c & 0xE0) == 0xE0) len = 3;
        else if ((c & 0xC0) == 0xC0) len = 2;
        if (i + len > byte_len) break;
        w += r ? r->measure_text_width(wr.text.substr(i, len), wr.font_size, family)
               : wr.font_size * 0.5f;
        i += len;
    }
    return w;
}

std::string markdown::link_at(float x, float y) const {
    for (const auto& wr : word_rects_) {
        if (!wr.href.empty() && x >= wr.x && x <= wr.x + wr.w &&
            y >= wr.y && y <= wr.y + wr.h)
            return wr.href;
    }
    return {};
}

size_t markdown::hit_test_text(float x, float y) const {
    if (word_rects_.empty()) return 0;
    size_t best = text.size();
    bool found = false;
    for (const auto& wr : word_rects_) {
        if (x >= wr.x && x <= wr.x + wr.w && y >= wr.y && y <= wr.y + wr.h)
            return wr.start + word_offset_at(wr, x - wr.x, measure_renderer());
        if (y >= wr.y && y <= wr.y + wr.h) {
            if (!found || x < wr.x) { best = wr.start; found = true; }
        }
    }
    return found ? best : (y < 0.0f ? 0 : text.size());
}

void markdown::draw_selection_highlight(std::shared_ptr<renderer> r) const {
    if (!has_selection()) return;
    size_t a = std::min(sel_anchor_, sel_pos_);
    size_t b = std::max(sel_anchor_, sel_pos_);
    const color sel_c = {0.20f, 0.40f, 0.80f, 0.35f};
    for (const auto& wr : word_rects_) {
        if (wr.end <= a || wr.start >= b) continue;
        size_t la = (a > wr.start ? a : wr.start) - wr.start;
        size_t lb = (b < wr.end ? b : wr.end) - wr.start;
        float xa = word_prefix_width(wr, la, r);
        float xb = word_prefix_width(wr, lb, r);
        r->draw_rectangle({wr.x + xa, wr.y, std::max(xb - xa, 1.0f), wr.h}, sel_c);
    }
}

void markdown::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        const float mx = md->position.x;
        const float my = md->position.y;
        if (md->action == mouse_action::down && md->button == mouse_button::left) {
            pressed_link_ = link_at(mx, my);
        } else if (md->action == mouse_action::up && md->button == mouse_button::left &&
                   !pressed_link_.empty()) {
            if (link_at(mx, my) == pressed_link_)
                platform::open_url(pressed_link_);
            pressed_link_.clear();
        }
    }
    label::handle_event(type, data);
}

void markdown::paint(std::shared_ptr<renderer> renderer) {
    cached_renderer_ = renderer;
    ensure_parsed();
    word_rects_.clear();
    float y = 0.0f;
    float avail_w = effective_width();
    for (const auto& b : blocks_)
        draw_block(renderer, b, 0.0f, y, avail_w, y);
    draw_selection_highlight(renderer);
}

} // namespace spiration
