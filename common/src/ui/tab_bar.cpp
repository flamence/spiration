/**
 * @file tab_bar.cpp
 * @brief 标签栏控件实现，支持可滚动标签头、动画、指示条渐显隐。
 * @author clk
 */

#include <ui/tab_bar.h>
#include <ui/theme.h>
#include <cmath>
#include <algorithm>

namespace spiration {

tab_head_item::tab_head_item(const std::string& title)
    : title_(title)
    , bg_(theme::get(theme::TAB_INACTIVE_BG))
    , text_(theme::get(theme::TAB_INACTIVE_TEXT))
    , close_fg_(theme::get(theme::TAB_CLOSE_FG)) {}

void tab_head_item::sync_colors() {
    if (active_) {
        bg_.animate_to(theme::get(theme::TAB_ACTIVE_BG), 150.0f);
        text_.animate_to(theme::get(theme::TAB_ACTIVE_TEXT), 150.0f);
    } else if (hovering_) {
        bg_.animate_to(theme::get(theme::TAB_HOVER_BG), 100.0f);
        text_.animate_to(theme::get(theme::TAB_HOVER_TEXT), 100.0f);
    } else {
        bg_.animate_to(theme::get(theme::TAB_INACTIVE_BG), 100.0f);
        text_.animate_to(theme::get(theme::TAB_INACTIVE_TEXT), 100.0f);
    }
}

void tab_head_item::tick(float dt_ms) {
    bool need = false;
    if (bg_.update(dt_ms)) need = true;
    if (text_.update(dt_ms)) need = true;
    if (close_fg_.update(dt_ms)) need = true;
    if (need && request_repaint_) request_repaint_();
}

void tab_head_item::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        bool old_hover = hovering_;
        bool old_close = close_hovering_;

        hovering_ = (md->position.x >= 0.0f && md->position.x < width &&
                     md->position.y >= 0.0f && md->position.y < height);
        close_hovering_ = (hovering_ &&
                           md->position.x >= width - CLOSE_BTN_W &&
                           md->position.x < width);

        if (hovering_ != old_hover) {
            sync_colors();
            if (request_repaint_) request_repaint_();
        }

        if (close_hovering_ != old_close) {
            if (close_hovering_) {
                close_fg_.animate_to(theme::get(theme::TAB_CLOSE_HOVER_FG), 100.0f);
            } else {
                close_fg_.animate_to(theme::get(theme::TAB_CLOSE_FG), 100.0f);
            }
            if (request_repaint_) request_repaint_();
        }

        if (md->action == mouse_action::down) {
            if (close_hovering_) {
                md->consumed = true;
                if (on_close_) on_close_();
                return;
            }
            if (hovering_) {
                md->consumed = true;
                if (on_activate_) on_activate_();
                return;
            }
        }
        return;
    }
    widget::handle_event(type, data);
}

void tab_head_item::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({x, y, width, height}, bg_.current());

    renderer->draw_text_aligned(title_,
                                {x + 10.0f, y, width - CLOSE_BTN_W - 14.0f, height},
                                text_.current(),
                                text_alignment::left, vertical_alignment::center, 13.0f);

    float centerX = x + width - CLOSE_BTN_W * 0.5f;
    float centerY = y + height * 0.5f;
    float half = CLOSE_ICON_SIZE * 0.5f;
    color closeCol = close_fg_.current();
    renderer->draw_line({centerX - half, centerY - half},
                        {centerX + half, centerY + half}, closeCol, 1.5f);
    renderer->draw_line({centerX + half, centerY - half},
                        {centerX - half, centerY + half}, closeCol, 1.5f);
}

void tab_bar::init() {
    widget_style.background_color = theme::get(theme::TAB_BAR_BG);

    header_row_ = std::make_unique<scroll_row>();
    header_row_->set_child_width(TAB_FIXED_W);
    header_row_->x = 0.0f;
    header_row_->y = 0.0f;
    header_row_->height = TAB_HEADER_H;
    header_row_->init();
}

void tab_bar::layout() {
    header_row_->width = width;
    header_row_->height = TAB_HEADER_H;
    header_row_->layout();

    float h = height - TAB_HEADER_H;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        tabs_[i]->x = 0.0f;
        tabs_[i]->y = TAB_HEADER_H;
        tabs_[i]->width = width;
        tabs_[i]->height = (static_cast<int>(i) == active_index_) ? h : 0.0f;
        tabs_[i]->layout();
    }
}

void tab_bar::tick(float dt_ms) {
    bool need_repaint = false;

    if (header_row_) header_row_->tick(dt_ms);

    if (prev_indicator_alpha_.update(dt_ms)) need_repaint = true;
    if (curr_indicator_alpha_.update(dt_ms)) need_repaint = true;

    for (auto& t : tabs_) {
        if (t) t->tick(dt_ms);
    }

    if (need_repaint && request_repaint_) request_repaint_();

    container::tick(dt_ms);
}

void tab_bar::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        bool header = (md->position.y >= 0.0f && md->position.y < TAB_HEADER_H);
        if (header) {
            point old = md->position;
            md->position.x = old.x - header_row_->x;
            md->position.y = old.y - header_row_->y;
            header_row_->handle_event(type, data);
            md->position = old;
            if (md->consumed) return;
        }

        if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
            auto* t = tabs_[active_index_].get();
            if (md->position.y >= TAB_HEADER_H) {
                point old = md->position;
                md->position.x = old.x - t->x;
                md->position.y = old.y - t->y;
                t->handle_event(type, data);
                md->position = old;
            }
        }
        return;
    }

    if (type == event_type::keyboard) {
        if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
            tabs_[active_index_]->handle_event(type, data);
        }
        return;
    }

    container::handle_event(type, data);
}

void tab_bar::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({x, y, width, TAB_HEADER_H}, widget_style.background_color);

    if (header_row_) {
        renderer->push_transform(x, y);
        header_row_->paint(renderer);
        renderer->pop_transform();
    }

    float scrollOff = header_row_ ? header_row_->scroll_offset() : 0.0f;

    color indicator_col = theme::get(theme::TAB_INDICATOR);

    float prevAlpha = prev_indicator_alpha_.current().a;
    if (prevAlpha > 0.005f && prev_indicator_width_ > 0.0f) {
        indicator_col.a = prevAlpha;
        renderer->draw_rectangle({x + prev_indicator_pos_ - scrollOff, y + TAB_HEADER_H - 2.0f,
                                  prev_indicator_width_, 2.0f},
                                 indicator_col);
    }

    float currAlpha = curr_indicator_alpha_.current().a;
    if (currAlpha > 0.005f && curr_indicator_width_ > 0.0f) {
        indicator_col.a = currAlpha;
        renderer->draw_rectangle({x + curr_indicator_pos_ - scrollOff, y + TAB_HEADER_H - 2.0f,
                                  curr_indicator_width_, 2.0f},
                                 indicator_col);
    }

    if (active_index_ >= 0 && active_index_ < tab_count()) {
        auto* t = tabs_[active_index_].get();
        renderer->push_transform(x, y);
        t->paint(renderer);
        renderer->pop_transform();
    }
}

void tab_bar::start_indicator_fade(float new_pos, float new_width) {
    prev_indicator_pos_ = curr_indicator_pos_;
    prev_indicator_width_ = curr_indicator_width_;
    prev_indicator_alpha_.snap_to(color::white());
    prev_indicator_alpha_.animate_to(color::transparent(), 180.0f);

    curr_indicator_pos_ = new_pos;
    curr_indicator_width_ = new_width;
    curr_indicator_alpha_.snap_to(color::transparent());
    curr_indicator_alpha_.animate_to(color::white(), 180.0f);
}

void tab_bar::add_tab(std::unique_ptr<tab> t) {
    if (!t) return;
    t->height = 0.0f;
    tabs_.push_back(std::move(t));

    auto* raw_tab = tabs_.back().get();

    if (request_repaint_) raw_tab->set_repaint_callback(request_repaint_);
    if (window_action_) raw_tab->set_window_action_callback(window_action_);

    auto head = std::make_unique<tab_head_item>(raw_tab->title_);
    tab_head_item* raw_head = head.get();
    head->height = TAB_HEADER_H;

    tab_heads_.push_back(raw_head);
    header_row_->add_child(std::move(head));

    raw_head->set_on_activate([this, raw_head]() {
        for (int i = 0; i < static_cast<int>(tab_heads_.size()); ++i) {
            if (tab_heads_[i] == raw_head) { activate_tab(i); return; }
        }
    });
    raw_head->set_on_close([this, raw_head]() {
        for (int i = 0; i < static_cast<int>(tab_heads_.size()); ++i) {
            if (tab_heads_[i] == raw_head) { close_tab(i); return; }
        }
    });

    if (tab_heads_.size() == 1) {
        raw_head->set_active(true);
        raw_head->bg_.snap_to(theme::get(theme::TAB_ACTIVE_BG));
        raw_head->text_.snap_to(theme::get(theme::TAB_ACTIVE_TEXT));
    }

    if (active_index_ < 0) activate_tab(0);
    if (request_repaint_) request_repaint_();
}

void tab_bar::activate_tab(int index) {
    if (index == active_index_) return;

    if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
        tabs_[active_index_]->active_ = false;
        tabs_[active_index_]->on_deactivate();
        if (active_index_ < static_cast<int>(tab_heads_.size())) {
            tab_heads_[active_index_]->set_active(false);
            tab_heads_[active_index_]->sync_colors();
        }
    }
    active_index_ = index;
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        tabs_[index]->active_ = true;
        tabs_[index]->on_activate();
        if (index < static_cast<int>(tab_heads_.size())) {
            tab_heads_[index]->set_active(true);
            tab_heads_[index]->sync_colors();
        }
    }

    float tw = TAB_FIXED_W;
    start_indicator_fade(static_cast<float>(active_index_) * tw, tw);

    layout();
    if (request_repaint_) request_repaint_();
}

void tab_bar::close_tab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

    tabs_.erase(tabs_.begin() + index);

    if (index < static_cast<int>(tab_heads_.size()) && tab_heads_[index]) {
        header_row_->remove_child(tab_heads_[index]);
        tab_heads_.erase(tab_heads_.begin() + index);
    }

    if (tabs_.empty()) {
        active_index_ = -1;
        prev_indicator_alpha_.snap_to(color::transparent());
        curr_indicator_alpha_.snap_to(color::transparent());
        prev_indicator_width_ = 0.0f;
        curr_indicator_width_ = 0.0f;
    } else {
        int old_active = active_index_;
        if (index < active_index_) {
            --active_index_;
        } else if (active_index_ >= static_cast<int>(tabs_.size())) {
            active_index_ = static_cast<int>(tabs_.size()) - 1;
        }

        if (active_index_ >= 0 && active_index_ < static_cast<int>(tab_heads_.size())) {
            tab_heads_[active_index_]->set_active(true);
            tab_heads_[active_index_]->sync_colors();
        }

        float tw = TAB_FIXED_W;
        start_indicator_fade(static_cast<float>(active_index_) * tw, tw);
    }

    layout();
    if (request_repaint_) request_repaint_();
}

} 
