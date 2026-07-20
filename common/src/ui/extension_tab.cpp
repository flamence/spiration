/**
 * @file extension_tab.cpp
 * @brief 扩展管理标签页实现�?
 * @author clk
 */

#include <ui/extension_tab.h>
#include <ui/theme_manager.h>
#include <extension/extension.h>
#include <extension/extension_manager.h>
#include <utils/i18n.h>
#include <cmath>

namespace spiration {

extension_tab::extension_tab() {
    widget_style.background_color = theme_manager::get(theme_manager::CONTENT_BG);
    title_ = i18n::tr("extensions");
    collect_extensions();
}

void extension_tab::on_activate() {
    collect_extensions();
    if (request_repaint_) request_repaint_();
}

void extension_tab::collect_extensions() {
    extensions_.clear();

    auto exts = extension_manager::extensions();
    for (const auto* ext : exts) {
        if (ext) {
            extensions_.push_back({ext->name(), ext->version(), ext->description()});
        }
    }
}

void extension_tab::paint(std::shared_ptr<renderer> renderer) {
    auto lh = [](float fs) { return std::ceil(fs * 1.6f); };

    float left_margin = 20.0f;
    float top_margin = 20.0f;
    float section_gap = 12.0f;

    float title_h = lh(18.0f);
    renderer->draw_text_aligned(
        i18n::tr("extensions_title"),
        {x + left_margin, y + top_margin, width - left_margin * 2, title_h},
        theme_manager::get(theme_manager::POPUP_TEXT),
        text_alignment::left, vertical_alignment::center, 18.0f);

    float cur_y = y + top_margin + title_h + 10.0f;

    if (extensions_.empty()) {
        float empty_h = lh(14.0f);
        renderer->draw_text_aligned(
            i18n::tr("no_extensions"),
            {x + left_margin, cur_y, width - left_margin * 2, empty_h},
            theme_manager::get(theme_manager::TEXT_MUTED),
            text_alignment::left, vertical_alignment::center, 14.0f);

        cur_y += empty_h + 4.0f;

        float hint_h = lh(12.0f);
        renderer->draw_text_aligned(
            i18n::tr("extensions_hint"),
            {x + left_margin, cur_y, width - left_margin * 2, hint_h},
            theme_manager::get(theme_manager::TEXT_MUTED),
            text_alignment::left, vertical_alignment::center, 12.0f);
        return;
    }

    float name_col = 200.0f;
    float ver_col = 100.0f;
    float desc_col = width - left_margin * 2 - name_col - ver_col;

    float header_h = lh(13.0f);
    renderer->draw_text_aligned(
        i18n::tr("ext_name"),
        {x + left_margin, cur_y, name_col, header_h},
        theme_manager::get(theme_manager::SEPARATOR),
        text_alignment::left, vertical_alignment::center, 13.0f);
    renderer->draw_text_aligned(
        i18n::tr("ext_version"),
        {x + left_margin + name_col, cur_y, ver_col, header_h},
        theme_manager::get(theme_manager::SEPARATOR),
        text_alignment::left, vertical_alignment::center, 13.0f);
    renderer->draw_text_aligned(
        i18n::tr("ext_description"),
        {x + left_margin + name_col + ver_col, cur_y, desc_col, header_h},
        theme_manager::get(theme_manager::SEPARATOR),
        text_alignment::left, vertical_alignment::center, 13.0f);

    cur_y += header_h + 4.0f;

    renderer->draw_line(
        {x + left_margin, cur_y},
        {x + width - left_margin, cur_y},
        theme_manager::get(theme_manager::SEPARATOR), 1.0f);

    cur_y += section_gap;

    float item_h_base = lh(14.0f) + 4.0f;
    item_heights_.resize(extensions_.size());
    for (size_t i = 0; i < extensions_.size(); ++i) {
        const auto& ext = extensions_[i];
        float item_y = cur_y;

        int desc_lines = 0;
        if (!ext.description.empty() && desc_col > 0.0f) {
            float desc_text_w = renderer->measure_text_width(ext.description, 13.0f);
            desc_lines = std::max(1, static_cast<int>(std::ceil(desc_text_w / desc_col)));
        }
        float desc_h = (desc_lines > 0) ? lh(13.0f) * desc_lines : 0.0f;
        float item_h = std::max(item_h_base, desc_h);
        item_heights_[i] = item_h;

        if (static_cast<int>(i) == hovered_index_ ||
            (hovered_index_ < 0 && static_cast<int>(i) == last_hovered_index_)) {
            renderer->draw_rectangle(
                {x + left_margin - 4.0f, item_y, width - left_margin * 2 + 8.0f, item_h},
                hover_bg_.current());
        }

        renderer->draw_text_aligned(
            ext.name,
            {x + left_margin, item_y, name_col, item_h},
            theme_manager::get(theme_manager::POPUP_TEXT),
            text_alignment::left, vertical_alignment::center, 14.0f);

        renderer->draw_text_aligned(
            "v" + ext.version,
            {x + left_margin + name_col, item_y, ver_col, item_h},
            theme_manager::get(theme_manager::POPUP_TEXT),
            text_alignment::left, vertical_alignment::center, 14.0f);

        if (!ext.description.empty()) {
            renderer->draw_text_aligned(
                ext.description,
                {x + left_margin + name_col + ver_col, item_y, desc_col, item_h},
                theme_manager::get(theme_manager::TEXT_MUTED),
                text_alignment::left, vertical_alignment::center, 13.0f);
        }

        cur_y += item_h;
    }
}

void extension_tab::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        auto lh = [](float fs) { return std::ceil(fs * 1.6f); };

        float left_margin = 20.0f;
        float top_margin = 20.0f;

        float title_h = lh(18.0f);
        float header_h = lh(13.0f);
        float section_gap = 12.0f;
        float item_h_base = lh(14.0f) + 4.0f;

        float list_y = top_margin + title_h + 10.0f + header_h + 4.0f + section_gap;
        float mx = md->position.x;
        float my = md->position.y;

        int old_hover = hovered_index_;
        hovered_index_ = -1;

        bool hit_item = false;
        if (mx >= left_margin && mx <= width - left_margin) {
            float cur_y = list_y;
            for (size_t i = 0; i < extensions_.size(); ++i) {
                float item_h = (i < item_heights_.size()) ? item_heights_[i] : item_h_base;
                if (my >= cur_y && my < cur_y + item_h) {
                    hovered_index_ = static_cast<int>(i);
                    hit_item = true;
                    break;
                }
                cur_y += item_h;
            }
        }

        if (hovered_index_ != old_hover) {
            if (hovered_index_ >= 0) {
                last_hovered_index_ = hovered_index_;
                hover_bg_.animate_to(theme_manager::get(theme_manager::POPUP_HOVER), 100.0f);
            } else {
                hover_bg_.animate_to(color::transparent(), 100.0f);
            }
            if (request_repaint_) request_repaint_();
        }

        if (hit_item) {
            md->consumed = true;
        }
        return;
    }
    container::handle_event(type, data);
}

void extension_tab::tick(float dt_ms) {
    if (hover_bg_.update(dt_ms) && request_repaint_) request_repaint_();
    tab::tick(dt_ms);
}

} // namespace spiration
