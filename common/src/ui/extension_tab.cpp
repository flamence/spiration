/**
 * @file extension_tab.cpp
 * @brief 拓展管理标签页实现。
 * @author clk
 */

#include <ui/extension_tab.h>
#include <ui/theme_manager.h>
#include <application.h>
#include <extension/extension.h>
#include <extension/extension_manager.h>
#include <extension/builtin/i18n/i18n.h>
#include <cmath>

namespace spiration {

extension_tab::extension_tab() {
    widget_style.background_color = theme_manager::get(theme_manager::CONTENT_BG);
    widget_style.overflow_y = true;
    title_ = i18n_manager::get().tr("menu.help.extensions");
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
    float header_h = lh(13.0f);
    float item_h_base = lh(14.0f) + 4.0f;

    float name_col = 200.0f;
    float ver_col = 100.0f;
    float desc_col = std::max(0.0f, width - left_margin * 2 - name_col - ver_col);

    float cur_y = top_margin + title_h + 10.0f;
    float content_h = cur_y;
    if (extensions_.empty()) {
        content_h = cur_y + lh(14.0f) + 4.0f + lh(12.0f) + 20.0f;
    } else {
        cur_y += header_h + 4.0f + section_gap;
        item_heights_.resize(extensions_.size());
        for (size_t i = 0; i < extensions_.size(); ++i) {
            const auto& ext = extensions_[i];
            int desc_lines = 0;
            if (!ext.description.empty() && desc_col > 0.0f) {
                float desc_text_w = renderer->measure_text_width(ext.description, 13.0f);
                desc_lines = std::max(1, static_cast<int>(std::ceil(desc_text_w / desc_col)));
            }
            float desc_h = (desc_lines > 0) ? lh(13.0f) * desc_lines : 0.0f;
            float item_h = std::max(item_h_base, desc_h);
            item_heights_[i] = item_h;
            cur_y += item_h;
        }
        content_h = cur_y + 20.0f;
    }
    set_scroll_content(width, content_h);

    renderer->push_clip({0, 0, width, height});
    renderer->push_transform(0, -scroll_offset_y());

    renderer->draw_text_aligned(
        i18n_manager::get().tr("extensions_title"),
        {left_margin, top_margin, width - left_margin * 2, title_h},
        theme_manager::get(theme_manager::POPUP_TEXT),
        text_alignment::left, vertical_alignment::center, 18.0f);

    float draw_y = top_margin + title_h + 10.0f;

    if (extensions_.empty()) {
        float empty_h = lh(14.0f);
        renderer->draw_text_aligned(
            i18n_manager::get().tr("no_extensions"),
            {left_margin, draw_y, width - left_margin * 2, empty_h},
            theme_manager::get(theme_manager::TEXT_MUTED),
            text_alignment::left, vertical_alignment::center, 14.0f);

        draw_y += empty_h + 4.0f;

        float hint_h = lh(12.0f);
        renderer->draw_text_aligned(
            i18n_manager::get().tr("extensions_hint"),
            {left_margin, draw_y, width - left_margin * 2, hint_h},
            theme_manager::get(theme_manager::TEXT_MUTED),
            text_alignment::left, vertical_alignment::center, 12.0f);
    } else {
        renderer->draw_text_aligned(
            i18n_manager::get().tr("ext_name"),
            {left_margin, draw_y, name_col, header_h},
            theme_manager::get(theme_manager::SEPARATOR),
            text_alignment::left, vertical_alignment::center, 13.0f);
        renderer->draw_text_aligned(
            i18n_manager::get().tr("ext_version"),
            {left_margin + name_col, draw_y, ver_col, header_h},
            theme_manager::get(theme_manager::SEPARATOR),
            text_alignment::left, vertical_alignment::center, 13.0f);
        renderer->draw_text_aligned(
            i18n_manager::get().tr("ext_description"),
            {left_margin + name_col + ver_col, draw_y, desc_col, header_h},
            theme_manager::get(theme_manager::SEPARATOR),
            text_alignment::left, vertical_alignment::center, 13.0f);

        draw_y += header_h + 4.0f;

        renderer->draw_line(
            {left_margin, draw_y},
            {width - left_margin, draw_y},
            theme_manager::get(theme_manager::SEPARATOR), 1.0f);

        draw_y += section_gap;

        for (size_t i = 0; i < extensions_.size(); ++i) {
            const auto& ext = extensions_[i];
            float item_h = (i < item_heights_.size()) ? item_heights_[i] : item_h_base;
            float item_y = draw_y;

            if (static_cast<int>(i) == hovered_index_ ||
                (hovered_index_ < 0 && static_cast<int>(i) == last_hovered_index_)) {
                renderer->draw_rectangle(
                    {left_margin - 4.0f, item_y, width - left_margin * 2 + 8.0f, item_h},
                    hover_bg_.current());
            }

            renderer->draw_text_aligned(
                ext.name,
                {left_margin, item_y, name_col, item_h},
                theme_manager::get(theme_manager::POPUP_TEXT),
                text_alignment::left, vertical_alignment::center, 14.0f);

            renderer->draw_text_aligned(
                "v" + ext.version,
                {left_margin + name_col, item_y, ver_col, item_h},
                theme_manager::get(theme_manager::POPUP_TEXT),
                text_alignment::left, vertical_alignment::center, 14.0f);

            if (!ext.description.empty()) {
                renderer->draw_text_aligned(
                    ext.description,
                    {left_margin + name_col + ver_col, item_y, desc_col, item_h},
                    theme_manager::get(theme_manager::TEXT_MUTED),
                    text_alignment::left, vertical_alignment::center, 13.0f);
            }

            draw_y += item_h;
        }
    }

    renderer->pop_transform();
    renderer->pop_clip();

    draw_scrollbars(renderer);
}

void extension_tab::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        if (md->action == mouse_action::wheel) {
            container::handle_event(type, data);
            return;
        }
        auto lh = [](float fs) { return std::ceil(fs * 1.6f); };

        float left_margin = 20.0f;
        float top_margin = 20.0f;

        float title_h = lh(18.0f);
        float header_h = lh(13.0f);
        float section_gap = 12.0f;
        float item_h_base = lh(14.0f) + 4.0f;

        float list_y = top_margin + title_h + 10.0f + header_h + 4.0f + section_gap;
        float mx = md->position.x;
        float my = md->position.y + scroll_offset_y();

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

        container::handle_event(type, data);
        return;
    }
    container::handle_event(type, data);
}

void extension_tab::tick(float dt_ms) {
    if (hover_bg_.update(dt_ms) && request_repaint_) request_repaint_();
    tab::tick(dt_ms);
}

} // namespace spiration
