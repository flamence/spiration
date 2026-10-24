/**
 * @file tab_bar.cpp
 * @brief 标签栏控件实现。
 * @author clk
 */

#include <spiration/ui/tab_bar.h>
#include <spiration/ui/button.h>
#include <spiration/ui/tab_head_item.h>
#include <spiration/ui/theme_manager.h>

#include <algorithm>

namespace spiration {

void tab_bar::init() {
    widget_style.background_color = theme_manager::get(theme_manager::TAB_BAR_BG);

    header_row_ = std::make_unique<scroll_row>();
    header_row_->set_child_width(TAB_FIXED_W);
    header_row_->x = 0.0f;
    header_row_->y = 0.0f;
    header_row_->height = TAB_HEADER_H;
    header_row_->init();
    header_row_->set_parent(this);

    actions_ = std::make_unique<container>();
    actions_->widget_style.background_color = color::transparent();
    actions_->set_parent(this);

    auto plus = std::make_unique<button>();
    plus->text = "+";
    plus->h_align = text_alignment::center;
    plus->widget_style.padding = margin(0, 0);
    plus->set_base_bg(color::transparent());
    plus->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    plus->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    plus->width = HEADER_ACTION_SIZE;
    plus->height = HEADER_ACTION_SIZE;
    plus->on_click = [this]() { if (on_new_tab_) on_new_tab_(); };
    plus->visible = false;
    new_btn_ = plus.get();
    actions_->add_child(std::move(plus));
}

float tab_bar::actions_width() const {
    int visible = 0;
    for (const auto& child : actions_->children()) {
        if (child->visible) ++visible;
    }
    if (visible == 0) return 0.0f;
    return HEADER_ACTION_PAD * 2.0f +
           static_cast<float>(visible) * HEADER_ACTION_SIZE +
           static_cast<float>(visible - 1) * HEADER_ACTION_GAP;
}

void tab_bar::layout_actions() {
    const float aw = actions_width();
    actions_->x = width - aw;
    actions_->y = 0.0f;
    actions_->width = aw;
    actions_->height = TAB_HEADER_H;

    float x = HEADER_ACTION_PAD;
    const float y = (TAB_HEADER_H - HEADER_ACTION_SIZE) * 0.5f;
    for (auto& child : actions_->children()) {
        if (!child->visible) continue;
        child->x = x;
        child->y = y;
        child->width = HEADER_ACTION_SIZE;
        child->height = HEADER_ACTION_SIZE;
        if (child->needs_layout()) child->layout();
        x += HEADER_ACTION_SIZE + HEADER_ACTION_GAP;
    }
}

void tab_bar::layout() {
    on_layout_begin();
    layout_actions();

    header_row_->x = 0.0f;
    header_row_->width = std::max(0.0f, width - actions_width());
    header_row_->height = TAB_HEADER_H;
    if (header_row_->needs_layout()) header_row_->layout();

    // 只布局活动标签；非活动标签隐藏即可，切回时按脏标记重排一次。
    for (size_t i = 0; i < tabs_.size(); ++i) {
        const bool active = (static_cast<int>(i) == active_index_);
        tabs_[i]->x = 0.0f;
        tabs_[i]->y = TAB_HEADER_H;
        tabs_[i]->width = width;
        tabs_[i]->height = active ? std::max(0.0f, height - TAB_HEADER_H) : 0.0f;
        if (active && tabs_[i]->needs_layout()) tabs_[i]->layout();
    }

    // 让活动标签始终处于头部可视范围内（键盘切换/溢出时自动滚动）。
    if (header_row_ && active_index_ >= 0) {
        const float view = header_row_->width;
        const float off = header_row_->scroll_offset();
        const float left = static_cast<float>(active_index_) * TAB_FIXED_W;
        const float right = left + TAB_FIXED_W;
        if (left < off) {
            header_row_->set_scroll_offset(left);
        } else if (right > off + view) {
            header_row_->set_scroll_offset(right - view);
        }
    }
}

void tab_bar::apply_callbacks(widget& w) {
    if (request_repaint_) w.set_repaint_callback(request_repaint_);
    if (window_action_) w.set_window_action_callback(window_action_);
    if (context_menu_cb_) w.set_context_menu_callback(context_menu_cb_);
}

void tab_bar::set_repaint_callback(std::function<void()> cb) {
    widget::set_repaint_callback(cb);
    if (header_row_) apply_callbacks(*header_row_);
    if (actions_) apply_callbacks(*actions_);
    for (auto& t : tabs_) {
        if (t) apply_callbacks(*t);
    }
}

void tab_bar::set_window_action_callback(std::function<void(int)> cb) {
    widget::set_window_action_callback(cb);
    if (header_row_) apply_callbacks(*header_row_);
    if (actions_) apply_callbacks(*actions_);
    for (auto& t : tabs_) {
        if (t) apply_callbacks(*t);
    }
}

void tab_bar::add_header_action(std::unique_ptr<widget> w) {
    if (!w || !actions_) return;
    apply_callbacks(*w);
    actions_->add_child(std::move(w));
    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

void tab_bar::clear_header_actions() {
    if (!actions_) return;
    std::vector<widget*> stale;
    for (const auto& child : actions_->children()) {
        if (child.get() != new_btn_) stale.push_back(child.get());
    }
    for (auto* w : stale) actions_->remove_child(w);
    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

void tab_bar::set_new_tab_callback(std::function<void()> cb) {
    on_new_tab_ = std::move(cb);
    if (new_btn_) {
        new_btn_->visible = static_cast<bool>(on_new_tab_);
        invalidate_layout();
        if (request_repaint_) request_repaint_();
    }
}

void tab_bar::reap_closed_tabs() {
    pending_close_heads_.clear();
    pending_close_tabs_.clear();
}

void tab_bar::tick(float dt_ms) {
    bool need_repaint = false;

    if (header_row_) header_row_->tick(dt_ms);
    if (actions_) actions_->tick(dt_ms);

    if (prev_indicator_alpha_.update(dt_ms)) need_repaint = true;
    if (curr_indicator_alpha_.update(dt_ms)) need_repaint = true;
    if (content_fade_.update(dt_ms)) need_repaint = true;

    // 非活动标签也要 tick：后台对话仍需消费流式事件并保存。
    for (auto& t : tabs_) {
        if (t) t->tick(dt_ms);
    }

    if (need_repaint && request_repaint_) request_repaint_();

    // 事件派发结束后再真正销毁已关闭的标签页，避免在其事件处理栈中 delete-this。
    reap_closed_tabs();
}

widget* tab_bar::hit_test_hover(float x, float y) const {
    if (!enabled) return nullptr;
    if (x < 0.0f || x > width || y < 0.0f || y > height) return nullptr;

    if (actions_ && x >= actions_->x) {
        if (widget* h = actions_->hit_test_hover(x - actions_->x, y - actions_->y)) {
            return h;
        }
    }
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
        const point orig = md->position;

        if (orig.y >= 0.0f && orig.y < TAB_HEADER_H) {
            if (actions_ && orig.x >= actions_->x) {
                md->position.x = orig.x - actions_->x;
                md->position.y = orig.y - actions_->y;
                actions_->handle_event(type, data);
                md->position = orig;
                return;
            }
            if (header_row_) {
                md->position.x = orig.x - header_row_->x;
                md->position.y = orig.y - header_row_->y;
                header_row_->handle_event(type, data);
                md->position = orig;
                if (md->consumed) return;
            }
            return;
        }

        if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
            auto* t = tabs_[active_index_].get();
            md->position.x = orig.x - t->x;
            md->position.y = orig.y - t->y;
            t->handle_event(type, data);
            md->position = orig;
        }
        return;
    }

    if (type == event_type::keyboard) {
        if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
            tabs_[active_index_]->handle_event(type, data);
        }
        return;
    }

    // 输入法组合等其余事件同样只派发给活动标签页（tab 不是 children_）。
    if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
        tabs_[active_index_]->handle_event(type, data);
    }
    container::handle_event(type, data);
}

void tab_bar::paint(std::shared_ptr<renderer> renderer) {
    renderer->draw_rectangle({0, 0, width, TAB_HEADER_H}, widget_style.background_color);

    const float scroll_off = header_row_ ? header_row_->scroll_offset() : 0.0f;
    const float header_w = header_row_ ? header_row_->width : width;

    renderer->push_clip({0, 0, header_w, TAB_HEADER_H});
    if (header_row_) {
        renderer->push_transform(header_row_->x, header_row_->y);
        header_row_->paint(renderer);
        renderer->pop_transform();
    }

    color indicator_col = theme_manager::get(theme_manager::TAB_INDICATOR);
    const float prev_alpha = prev_indicator_alpha_.current().a;
    if (prev_alpha > 0.005f && prev_indicator_width_ > 0.0f) {
        indicator_col.a = prev_alpha;
        renderer->draw_rectangle({prev_indicator_pos_ - scroll_off, TAB_HEADER_H - 2.0f,
                                  prev_indicator_width_, 2.0f},
                                 indicator_col);
    }
    const float curr_alpha = curr_indicator_alpha_.current().a;
    if (curr_alpha > 0.005f && curr_indicator_width_ > 0.0f) {
        indicator_col.a = curr_alpha;
        renderer->draw_rectangle({curr_indicator_pos_ - scroll_off, TAB_HEADER_H - 2.0f,
                                  curr_indicator_width_, 2.0f},
                                 indicator_col);
    }
    renderer->pop_clip();

    if (actions_) {
        renderer->push_transform(actions_->x, actions_->y);
        actions_->paint(renderer);
        renderer->pop_transform();
    }

    if (active_index_ >= 0 && active_index_ < tab_count()) {
        auto* t = tabs_[active_index_].get();
        renderer->push_clip({0, TAB_HEADER_H, width, height - TAB_HEADER_H});
        renderer->push_transform(0, TAB_HEADER_H);
        renderer->set_alpha(content_fade_.current());
        t->paint(renderer);
        renderer->set_alpha(1.0f);
        renderer->pop_transform();
        renderer->pop_clip();
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

int tab_bar::index_of_head(const tab_head_item* head) const {
    for (int i = 0; i < static_cast<int>(tab_heads_.size()); ++i) {
        if (tab_heads_[i] == head) return i;
    }
    return -1;
}

void tab_bar::add_tab(std::unique_ptr<tab> t) {
    if (!t) return;
    t->height = 0.0f;
    t->set_parent(this);
    tab* raw_tab = t.get();
    tabs_.push_back(std::move(t));
    apply_callbacks(*raw_tab);

    auto head = std::make_unique<tab_head_item>(raw_tab->title_);
    tab_head_item* raw_head = head.get();
    head->height = TAB_HEADER_H;
    tab_heads_.push_back(raw_head);
    header_row_->add_child(std::move(head));

    raw_head->set_on_activate([this, raw_head]() {
        const int index = index_of_head(raw_head);
        if (index >= 0) activate_tab(index);
    });
    raw_head->set_on_close([this, raw_head]() {
        const int index = index_of_head(raw_head);
        if (index >= 0) close_tab(index);
    });
    raw_head->on_context_menu = [this, raw_head](float x, float y) {
        const int index = index_of_head(raw_head);
        if (index >= 0 && on_tab_context_menu_) on_tab_context_menu_(index, x, y);
    };
    raw_tab->set_on_title_change([raw_head](const std::string& t) {
        raw_head->set_title(t);
    });

    // 新标签页直接激活（与浏览器/编辑器一致），首次添加时 active_index_ 为 -1。
    activate_tab(tab_count() - 1);

    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

void tab_bar::activate_tab(int index) {
    if (index < 0 || index >= tab_count() || index == active_index_) return;
    apply_active_state(index);
    start_indicator_fade(static_cast<float>(index) * TAB_FIXED_W, TAB_FIXED_W);
    content_fade_.snap_to(0.0f);
    content_fade_.animate_to(1.0f, 150.0f);
    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

void tab_bar::apply_active_state(int index) {
    if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
        tabs_[active_index_]->active_ = false;
        tabs_[active_index_]->on_deactivate();
        if (active_index_ < static_cast<int>(tab_heads_.size())) {
            tab_heads_[active_index_]->set_active(false);
        }
    }
    active_index_ = index;
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        tabs_[index]->active_ = true;
        tabs_[index]->on_activate();
        if (index < static_cast<int>(tab_heads_.size())) {
            tab_heads_[index]->set_active(true);
        }
    }
}

void tab_bar::close_tab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

    const bool was_active = (index == active_index_);
    pending_close_tabs_.push_back(std::move(tabs_[index]));
    tabs_.erase(tabs_.begin() + index);

    if (index < static_cast<int>(tab_heads_.size()) && tab_heads_[index]) {
        pending_close_heads_.push_back(header_row_->remove_child(tab_heads_[index]));
        tab_heads_.erase(tab_heads_.begin() + index);
    }

    if (tabs_.empty()) {
        active_index_ = -1;
        prev_indicator_alpha_.snap_to(color::transparent());
        curr_indicator_alpha_.snap_to(color::transparent());
        prev_indicator_width_ = 0.0f;
        curr_indicator_width_ = 0.0f;
        content_fade_.snap_to(1.0f);
    } else {
        if (index < active_index_) --active_index_;
        active_index_ = std::min(active_index_, tab_count() - 1);
        if (was_active) {
            const int old_active = active_index_;
            active_index_ = -1;
            apply_active_state(old_active);
            start_indicator_fade(static_cast<float>(old_active) * TAB_FIXED_W, TAB_FIXED_W);
        }
    }

    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

bool tab_bar::handle_shortcut(const key_event_data& key) {
    if (key.codepoint != 0 || key.key_code == 0 || !key.down || key.alt) return false;

    auto match = [&key](const tab_shortcut& s) {
        if (s.key_code == 0 || s.key_code != key.key_code) return false;
        if (key.shift != s.shift) return false;
        return s.ctrl_only ? key.ctrl : primary_modifier_down(key);
    };

    if (match(shortcuts_.close)) {
        if (active_index_ < 0) return false;
        close_tab(active_index_);
        return true;
    }
    if (match(shortcuts_.new_tab)) {
        if (!on_new_tab_) return false;
        on_new_tab_();
        return true;
    }
    if (match(shortcuts_.next) || match(shortcuts_.prev)) {
        if (tab_count() < 2) return false;
        const int step = match(shortcuts_.next) ? 1 : -1;
        activate_tab((active_index_ + step + tab_count()) % tab_count());
        return true;
    }
    if (shortcuts_.number_keys && primary_modifier_down(key) && !key.shift) {
        if (key.key_code >= vk::Digit1 && key.key_code <= vk::Digit9 && tab_count() > 0) {
            const int index = (key.key_code == vk::Digit9)
                                  ? tab_count() - 1
                                  : key.key_code - vk::Digit1;
            if (index < tab_count()) {
                activate_tab(index);
                return true;
            }
        }
    }
    return false;
}

} // namespace spiration
