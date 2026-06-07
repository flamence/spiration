/**
 * @file popup_menu.cpp
 * @brief 弹出菜单控件实现。
 * @author clk
 */

#include <ui/popup_menu.h>
#include <ui/layout.h>
#include <ui/button.h>
#include <ui/theme.h>

namespace spiration {

void popup_menu::init() {
    style.background_color = theme::popup_bg();
    auto vlayout = std::make_unique<vertical_layout>(0.0f);
    set_layout_manager(std::move(vlayout));
}

void popup_menu::add_item(const std::string& text, std::function<void()> callback) {
    popup_item item;
    item.text = text;
    item.callback = std::move(callback);
    items_.push_back(std::move(item));
    layout_items();
}

void popup_menu::layout_items() {
    float maxW = 120.0f;
    for (auto& item : items_) {
        maxW = std::max(maxW, static_cast<float>(item.text.size()) * 8.0f + 24.0f);
    }
    width = maxW;
    
    height = 0.0f;
    for (auto& item : items_) {
        height += (item.text == "---") ? 8.0f : item_height_;
    }
}

void popup_menu::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        
        int idx = -1;
        if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height) {
            float curY = 0.0f;
            for (size_t i = 0; i < items_.size(); ++i) {
                float h = (items_[i].text == "---") ? 8.0f : item_height_;
                if (my >= curY && my < curY + h) {
                    idx = static_cast<int>(i);
                    break;
                }
                curY += h;
            }
        }

        if (idx != hovered_index_) {
            hovered_index_ = idx;
            if (request_repaint_) request_repaint_();
        }

        
        if (md->action == mouse_action::down && idx >= 0) {
            md->consumed = true;
            if (items_[idx].text == "---") return; 
            auto cb = items_[idx].callback;
            if (on_dismiss_) on_dismiss_();
            if (cb) cb();
            return;
        }

        
        if (md->action == mouse_action::down && idx < 0) {
            if (on_dismiss_) on_dismiss_();
            return;
        }
    }
    container::handle_event(type, data);
}

void popup_menu::paint(std::shared_ptr<renderer> renderer) {
renderer->draw_rectangle({x, y, width, height}, theme::popup_bg());
    renderer->draw_rectangle_outline({x, y, width, height}, theme::popup_border(), 1.0f);

    float iy = y;
    for (size_t i = 0; i < items_.size(); ++i) {
        float h = (items_[i].text == "---") ? 8.0f : item_height_;
        if (items_[i].text == "---") {
            
            float cy = iy + h * 0.5f;
            renderer->draw_line({x + 8.0f, cy}, {x + width - 8.0f, cy}, theme::separator(), 1.0f);
        } else {
            if (static_cast<int>(i) == hovered_index_) {
                renderer->draw_rectangle({x, iy, width, h}, theme::popup_hover());
            }
            renderer->draw_text_aligned(items_[i].text, {x + 8.0f, iy, width - 16.0f, h},
                                        theme::popup_text(), text_alignment::left, vertical_alignment::center, 14.0f);
        }
        iy += h;
    }
}

} 
