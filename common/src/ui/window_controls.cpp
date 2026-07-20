/**
 * @file window_controls.cpp
 * @brief 平台感知的窗口控制按钮实现�?
 * @author clk
 */

#include <ui/window_controls.h>
#include <ui/theme_manager.h>

namespace spiration {

window_controls::hit_part window_controls::hit_test_btn(float mx, float my) const {
    if (my < 0.0f || my > height) return hit_part::none;
#ifdef __APPLE__
    if (mx >= 0.0f && mx < btn_size_) return hit_part::close;
    if (mx >= btn_size_ && mx < btn_size_ * 2) return hit_part::min;
    if (mx >= btn_size_ * 2 && mx < btn_size_ * 3) return hit_part::max;
#else
    float startX = width - btn_size_ * 3.0f;
    if (mx >= startX && mx < startX + btn_size_) return hit_part::min;
    if (mx >= startX + btn_size_ && mx < startX + btn_size_ * 2) return hit_part::max;
    if (mx >= startX + btn_size_ * 2 && mx < startX + btn_size_ * 3) return hit_part::close;
#endif
    return hit_part::none;
}

void window_controls::init() {
    widget_style.background_color = color::transparent();
    btn_h_ = height > 0 ? height : 34.0f;
}

void window_controls::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        hit_part part = hit_test_btn(mx, my);
        bool oldHover = hover_min_ || hover_max_ || hover_close_;
        hover_min_ = (part == hit_part::min);
        hover_max_ = (part == hit_part::max);
        hover_close_ = (part == hit_part::close);
        if (oldHover != (hover_min_ || hover_max_ || hover_close_)) {
            if (request_repaint_) request_repaint_();
        }

        if (md->action == mouse_action::down && part != hit_part::none) {
            md->consumed = true;
            int action = -1;
            switch (part) {
                case hit_part::min:   action = widget::action_minimize; break;
                case hit_part::max:   action = widget::action_maximize; break;
                case hit_part::close: action = widget::action_close;    break;
                default: break;
            }
            if (action >= 0 && window_action_) window_action_(action);
        }
    }
    container::handle_event(type, data);
}

void window_controls::paint(std::shared_ptr<renderer> renderer) {
    float h = btn_h_;
    constexpr float iconSize = 10.0f;
    float iconHalf = iconSize * 0.5f;
    
#ifdef __APPLE__
    float xs[3] = { (btn_size_ - iconSize) * 0.5f,
                    btn_size_ + (btn_size_ - iconSize) * 0.5f,
                    btn_size_ * 2 + (btn_size_ - iconSize) * 0.5f };
    color colors[3] = { {1,0.25f,0.25f}, {1,0.8f,0.1f}, {0.3f,0.85f,0.3f} };
    bool hovers[3] = { hover_close_, hover_min_, hover_max_ };
    for (int i = 0; i < 3; ++i) {
        color c = colors[i];
        if (hovers[i]) {
            c.r = std::min(c.r + 0.2f, 1.0f);
            c.g = std::min(c.g + 0.2f, 1.0f);
            c.b = std::min(c.b + 0.2f, 1.0f);
        }
        renderer->draw_circle({x + xs[i] + iconHalf, y + h * 0.5f}, iconHalf, c);
    }
#else
    float startX = width - btn_size_ * 3.0f;
    float cx[3] = { x + startX + btn_size_ * 0.5f,
                    x + startX + btn_size_ * 1.5f,
                    x + startX + btn_size_ * 2.5f };
    bool hovers[3] = { hover_min_, hover_max_, hover_close_ };

    for (int i = 0; i < 3; ++i) {
        float bx = x + startX + static_cast<float>(i) * btn_size_;
        if (i == 2) {
            color bg = hovers[i] ? theme_manager::get(theme_manager::CLOSE_HOVER) : color::transparent();
            renderer->draw_rectangle({bx, y, btn_size_, h}, bg);
        } else if (hovers[i]) {
            renderer->draw_rectangle({bx, y, btn_size_, h}, theme_manager::get(theme_manager::CONTROL_HOVER_BG));
        }
    }

    float cy = y + h * 0.5f;
    renderer->draw_line({cx[0] - 5.0f, cy}, {cx[0] + 5.0f, cy}, theme_manager::get(theme_manager::CONTROL_ICON), 1.5f);
    renderer->draw_rectangle_outline({cx[1] - 5.0f, cy - 4.0f, iconSize, iconSize - 1.0f}, theme_manager::get(theme_manager::CONTROL_ICON), 1.5f);
    color closeIcon = hover_close_ ? theme_manager::get(theme_manager::CONTROL_ICON_HOVER) : theme_manager::get(theme_manager::CONTROL_ICON);
    renderer->draw_line({cx[2] - 5.0f, cy - 5.0f}, {cx[2] + 5.0f, cy + 5.0f}, closeIcon, 1.5f);
    renderer->draw_line({cx[2] + 5.0f, cy - 5.0f}, {cx[2] - 5.0f, cy + 5.0f}, closeIcon, 1.5f);
#endif
}

} 
