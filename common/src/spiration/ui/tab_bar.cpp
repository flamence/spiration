/**
 * @file tab_bar.cpp
 * @brief 标签栏控件实现。
 * @author clk
 */

#include <spiration/ui/tab_bar.h>
#include <spiration/ui/tab_head_item.h>
#include <spiration/ui/focus_manager.h>
#include <spiration/ui/theme_manager.h>

namespace spiration {

void tab_bar::init() {
    widget_style.background_color = theme_manager::get(theme_manager::TAB_BAR_BG);

    header_row_ = std::make_unique<scroll_row>();
    header_row_->set_child_width(TAB_FIXED_W);
    header_row_->x = 0.0f;
    header_row_->y = 0.0f;
    header_row_->height = TAB_HEADER_H;
    header_row_->init();
}

void tab_bar::layout() {
    on_layout_begin();

    header_row_->width = width;
    header_row_->height = TAB_HEADER_H;
    if (header_row_->needs_layout()) header_row_->layout();

    float h = height - TAB_HEADER_H;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        tabs_[i]->x = 0.0f;
        tabs_[i]->y = TAB_HEADER_H;
        tabs_[i]->width = width;
        tabs_[i]->height = (static_cast<int>(i) == active_index_) ? h : 0.0f;
        if (tabs_[i]->needs_layout()) tabs_[i]->layout();
    }
}

void tab_bar::tick(float dt_ms) {
    bool need_repaint = false;

    if (header_row_) header_row_->tick(dt_ms);

    if (prev_indicator_alpha_.update(dt_ms)) need_repaint = true;
    if (curr_indicator_alpha_.update(dt_ms)) need_repaint = true;
    if (content_fade_.update(dt_ms)) need_repaint = true;

    for (auto& t : tabs_) {
        if (t) t->tick(dt_ms);
    }

    if (need_repaint && request_repaint_) request_repaint_();

    container::tick(dt_ms);
}

widget* tab_bar::hit_test_hover(float x, float y) const {
    if (!enabled) return nullptr;
    if (x < 0.0f || x > width || y < 0.0f || y > height) return nullptr;

    if (header_row_) {
        if (widget* h = header_row_->hit_test_hover(x - header_row_->x,
                                                    y - header_row_->y)) {
            return h;
        }
    }
    if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
        const auto* t = tabs_[static_cast<size_t>(active_index_)].get();
        if (widget* h = t->hit_test_hover(x - t->x, y - t->y)) {
            return h;
        }
    }
    return const_cast<tab_bar*>(this);
}

void tab_bar::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        point old = md->position;
        if (header_row_) {
            md->position.x = old.x - header_row_->x;
            md->position.y = old.y - header_row_->y;
            header_row_->handle_event(type, data);
            md->position = old;
        }

        bool header = (old.y >= 0.0f && old.y < TAB_HEADER_H);
        if (header && md->consumed) return;

        if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
            auto* t = tabs_[active_index_].get();
            if (old.y >= TAB_HEADER_H) {
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
    renderer->draw_rectangle({0, 0, width, TAB_HEADER_H}, widget_style.background_color);

    if (header_row_) {
        renderer->push_transform(0, 0);
        header_row_->paint(renderer);
        renderer->pop_transform();
    }

    float scrollOff = header_row_ ? header_row_->scroll_offset() : 0.0f;

    color indicator_col = theme_manager::get(theme_manager::TAB_INDICATOR);

    float prevAlpha = prev_indicator_alpha_.current().a;
    if (prevAlpha > 0.005f && prev_indicator_width_ > 0.0f) {
        indicator_col.a = prevAlpha;
        renderer->draw_rectangle({prev_indicator_pos_ - scrollOff, TAB_HEADER_H - 2.0f,
                                  prev_indicator_width_, 2.0f},
                                 indicator_col);
    }

    float currAlpha = curr_indicator_alpha_.current().a;
    if (currAlpha > 0.005f && curr_indicator_width_ > 0.0f) {
        indicator_col.a = currAlpha;
        renderer->draw_rectangle({curr_indicator_pos_ - scrollOff, TAB_HEADER_H - 2.0f,
                                  curr_indicator_width_, 2.0f},
                                 indicator_col);
    }

    if (active_index_ >= 0 && active_index_ < tab_count()) {
        auto* t = tabs_[active_index_].get();
        renderer->push_transform(0, TAB_HEADER_H);
        renderer->push_clip({0, 0, width, height - TAB_HEADER_H});
        renderer->set_alpha(content_fade_.current());
        t->paint(renderer);
        renderer->set_alpha(1.0f);
        renderer->pop_clip();
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
    t->set_parent(this);
    tabs_.push_back(std::move(t));

    auto* raw_tab = tabs_.back().get();

    if (request_repaint_) raw_tab->set_repaint_callback(request_repaint_);
    if (window_action_) raw_tab->set_window_action_callback(window_action_);

    if (request_repaint_ && header_row_) {
        header_row_->set_repaint_callback(request_repaint_);
    }
    if (window_action_ && header_row_) {
        header_row_->set_window_action_callback(window_action_);
    }

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

    raw_tab->set_on_title_change([raw_head](const std::string& t) {
        raw_head->set_title(t);
    });

    if (tab_heads_.size() == 1) {
        raw_head->set_active(true);
        raw_head->bg_.snap_to(theme_manager::get(theme_manager::TAB_ACTIVE_BG));
        raw_head->text_.snap_to(theme_manager::get(theme_manager::TAB_ACTIVE_TEXT));
    }

    if (active_index_ < 0) activate_tab(0);

    invalidate_layout();
    layout();
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

    content_fade_.snap_to(0.0f);
    content_fade_.animate_to(1.0f, 150.0f);

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

    invalidate_layout();
    layout();
    if (request_repaint_) request_repaint_();
}

} // namespace spiration
