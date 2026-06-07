/**
 * @file tab_bar.cpp
 * @brief 标签栏控件实现。
 * @author clk
 */

#include <ui/tab_bar.h>
#include <ui/theme.h>
#include <algorithm>

namespace spiration {

void tab_bar::init() {
    style.background_color = { 0.85f, 0.85f, 0.85f };
}

void tab_bar::layout() {
    float h = height - TAB_HEADER_H;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        tabs_[i]->x = 0.0f;
        tabs_[i]->y = TAB_HEADER_H;
        tabs_[i]->width = width;
        tabs_[i]->height = (static_cast<int>(i) == active_index_) ? h : 0.0f;
        tabs_[i]->layout();
    }
}

int tab_bar::hit_test_tab_header(float mx, float my) const {
    if (my < 0.0f || my > TAB_HEADER_H) return -1;
    float cx = 0.0f;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        float tw = std::max(TAB_MIN_W, width / static_cast<float>(std::max((size_t)1, tabs_.size())));
        if (mx >= cx && mx < cx + tw) return static_cast<int>(i);
        cx += tw;
    }
    return -1;
}

int tab_bar::hit_test_close_btn(float mx, float my) const {
    if (my < 0.0f || my > TAB_HEADER_H) return -1;
    float cx = 0.0f;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        float tw = std::max(TAB_MIN_W, width / static_cast<float>(std::max((size_t)1, tabs_.size())));
        if (mx >= cx + tw - CLOSE_BTN_W && mx < cx + tw)
            return static_cast<int>(i);
        cx += tw;
    }
    return -1;
}

void tab_bar::tick(float dt_ms) {
    
    container::tick(dt_ms);
}

void tab_bar::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        
        int oldHover = hovered_close_idx_;
        hovered_close_idx_ = hit_test_close_btn(md->position.x, md->position.y);
        if (oldHover != hovered_close_idx_ && request_repaint_) request_repaint_();

        if (md->action == mouse_action::down) {
            
            int closeIdx = hit_test_close_btn(md->position.x, md->position.y);
            if (closeIdx >= 0) {
                md->consumed = true;
                close_tab(closeIdx);
                return;
            }
            int idx = hit_test_tab_header(md->position.x, md->position.y);
            if (idx >= 0) {
                md->consumed = true;
                activate_tab(idx);
                return;
            }
        }

        
        if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
            auto* t = tabs_[active_index_].get();
            point old = md->position;
            md->position.x = old.x - t->x;
            md->position.y = old.y - t->y;
            t->handle_event(type, data);
            md->position = old;
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
    renderer->draw_rectangle({x, y, width, TAB_HEADER_H}, theme::appbar_bg());

    float cx = x;
    float tabW = std::max(TAB_MIN_W, width / static_cast<float>(std::max(1, tab_count())));
    for (int i = 0; i < tab_count(); ++i) {
        bool active = (i == active_index_);
        color bg = active ? theme::editor_bg() : color{0.68f, 0.68f, 0.68f};
        renderer->draw_rectangle({cx, y, tabW, TAB_HEADER_H}, bg);
        if (!active) {
            renderer->draw_line({cx, y + TAB_HEADER_H}, {cx + tabW, y + TAB_HEADER_H}, {0.55f, 0.55f, 0.55f}, 1.0f);
        }
        renderer->draw_text_aligned(tabs_[i]->title(), {cx + 4.0f, y, tabW - CLOSE_BTN_W - 6.0f, TAB_HEADER_H},
                                    active ? color{0.1f,0.1f,0.1f} : color{0.35f,0.35f,0.35f},
                                    text_alignment::left, vertical_alignment::center, 13.0f);

        
        float closeX = cx + tabW - CLOSE_BTN_W;
        color closeCol = (hovered_close_idx_ == i) ? color{0.85f, 0.2f, 0.2f} : color{0.4f, 0.4f, 0.4f};
        renderer->draw_line({closeX + 4.0f, y + 8.0f}, {closeX + CLOSE_BTN_W - 4.0f, y + TAB_HEADER_H - 8.0f}, closeCol, 1.5f);
        renderer->draw_line({closeX + CLOSE_BTN_W - 4.0f, y + 8.0f}, {closeX + 4.0f, y + TAB_HEADER_H - 8.0f}, closeCol, 1.5f);

        cx += tabW;
    }

    if (active_index_ >= 0 && active_index_ < tab_count()) {
        auto* t = tabs_[active_index_].get();
        renderer->push_transform(x, y);
        t->paint(renderer);
        renderer->pop_transform();
    }
}

void tab_bar::add_tab(std::unique_ptr<tab> t) {
    if (!t) return;
    t->height = 0.0f;
    tabs_.push_back(std::move(t));
    if (active_index_ < 0) activate_tab(0);
    if (request_repaint_) request_repaint_();
}

void tab_bar::activate_tab(int index) {
    if (index == active_index_) return;
    if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
        tabs_[active_index_]->active_ = false;
        tabs_[active_index_]->on_deactivate();
    }
    active_index_ = index;
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        tabs_[index]->active_ = true;
        tabs_[index]->on_activate();
    }
    layout();
    if (request_repaint_) request_repaint_();
}

void tab_bar::close_tab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;
    tabs_.erase(tabs_.begin() + index);
    if (tabs_.empty()) { active_index_ = -1; }
    else if (active_index_ >= static_cast<int>(tabs_.size())) active_index_ = static_cast<int>(tabs_.size()) - 1;
    layout();
    if (request_repaint_) request_repaint_();
}

} 
