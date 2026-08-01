/**
 * @file popup_menu.cpp
 * @brief 弹出菜单控件实现。
 * @author clk
 */

#include <ui/popup_menu.h>
#include <ui/layout.h>
#include <ui/button.h>
#include <ui/theme_manager.h>

namespace spiration {

void popup_menu::init() {
    widget_style.background_color = theme_manager::get(theme_manager::POPUP_BG);
}

void popup_menu::add_item(const std::string& text, std::function<void()> callback) {
    popup_item item;
    item.text = text;
    item.callback = std::move(callback);
    items_.push_back(std::move(item));
    layout_items();
}

void popup_menu::add_separator() {
    popup_item item;
    item.separator = true;
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
        height += item.separator ? 8.0f : item_height_;
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
                float h = items_[i].separator ? 8.0f : item_height_;
                if (my >= curY && my < curY + h) {
                    idx = static_cast<int>(i);
                    break;
                }
                curY += h;
            }
        }

        if (idx != hovered_index_) {
            hovered_index_ = idx;
            if (idx >= 0) {
                hover_bg_.animate_to(theme_manager::get(theme_manager::POPUP_HOVER), 100.0f);
            } else {
                hover_bg_.animate_to(color::transparent(), 100.0f);
            }
            if (request_repaint_) request_repaint_();
        }

        if (md->action == mouse_action::down && idx >= 0) {
            md->consumed = true;
            if (items_[idx].separator) return;
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

void popup_menu::tick(float dt_ms) {
    if (hover_bg_.update(dt_ms) && request_repaint_) request_repaint_();
    container::tick(dt_ms);
}

void popup_menu::paint(std::shared_ptr<renderer> renderer) {
    // root 已通过 push_transform 处理 (x,y)，此处用 (0,0) 基准
    renderer->draw_rectangle({0, 0, width, height}, theme_manager::get(theme_manager::POPUP_BG));

    float iy = 0;
    for (size_t i = 0; i < items_.size(); ++i) {
        float h = items_[i].separator ? 8.0f : item_height_;
        if (items_[i].separator) {
            float cy = iy + h * 0.5f;
            renderer->draw_line({8.0f, cy}, {width - 8.0f, cy}, theme_manager::get(theme_manager::SEPARATOR), 1.0f);
        } else {
            if (static_cast<int>(i) == hovered_index_) {
                renderer->draw_rectangle({0, iy, width, h}, hover_bg_.current());
            }
            renderer->draw_text_aligned(items_[i].text, {12.0f, iy, width - 18.0f, h},
                                        theme_manager::get(theme_manager::POPUP_TEXT), text_alignment::left, vertical_alignment::center, 14.0f);
        }
        iy += h;
    }
}

} // namespace spiration
