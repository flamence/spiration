/**
 * @file window_controls.cpp
 * @brief 平台感知的窗口控制按钮实现�?
 * @author clk
 */

#include <spiration/ui/window_controls.h>
#include <spiration/ui/theme_manager.h>

namespace spiration {

bool window_controls::hit_test(float x, float y) const {
    return hit_test_btn(x, y) != hit_part::none;
}

window_controls::hit_part window_controls::hit_test_btn(float mx, float my) const {
    if (my < 0.0f || my > height) return hit_part::none;
    float startX = width - btn_size_ * 3.0f;
    if (mx >= startX && mx < startX + btn_size_) return hit_part::min;
    if (mx >= startX + btn_size_ && mx < startX + btn_size_ * 2) return hit_part::max;
    if (mx >= startX + btn_size_ * 2 && mx < startX + btn_size_ * 3) return hit_part::close;
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
        bool changed = false;
        bool old_min = hover_min_;
        bool old_max = hover_max_;
        bool old_close = hover_close_;
        hover_min_ = (part == hit_part::min);
        hover_max_ = (part == hit_part::max);
        hover_close_ = (part == hit_part::close);
        if (hover_min_ != old_min) {
            hover_min_bg_.animate_to(hover_min_
                ? theme_manager::get(theme_manager::CONTROL_HOVER_BG)
                : color::transparent(), 100.0f);
            changed = true;
        }
        if (hover_max_ != old_max) {
            hover_max_bg_.animate_to(hover_max_
                ? theme_manager::get(theme_manager::CONTROL_HOVER_BG)
                : color::transparent(), 100.0f);
            changed = true;
        }
        if (hover_close_ != old_close) {
            close_bg_.animate_to(hover_close_
                ? theme_manager::get(theme_manager::CLOSE_HOVER)
                : color::transparent(), 100.0f);
            changed = true;
        }
        if (changed && request_repaint_) request_repaint_();

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

void window_controls::tick(float dt_ms) {
    if (hover_min_bg_.update(dt_ms) || hover_max_bg_.update(dt_ms) ||
        close_bg_.update(dt_ms)) {
        if (request_repaint_) request_repaint_();
    }
    container::tick(dt_ms);
}

void window_controls::paint(std::shared_ptr<renderer> renderer) {
    float h = btn_h_;
    constexpr float iconSize = 10.0f;
    float startX = width - btn_size_ * 3.0f;
    float cx[3] = { startX + btn_size_ * 0.5f,
                    startX + btn_size_ * 1.5f,
                    startX + btn_size_ * 2.5f };

    for (int i = 0; i < 3; ++i) {
        float bx = startX + static_cast<float>(i) * btn_size_;
        color bg = color::transparent();
        if (i == 0) bg = hover_min_bg_.current();
        else if (i == 1) bg = hover_max_bg_.current();
        else bg = close_bg_.current();
        renderer->draw_rectangle({bx, 0, btn_size_, h}, bg);
    }

    float cy = y + h * 0.5f;
    renderer->draw_line({cx[0] - 5.0f, cy}, {cx[0] + 5.0f, cy}, theme_manager::get(theme_manager::CONTROL_ICON), 1.5f);
    renderer->draw_rectangle_outline({cx[1] - 5.0f, cy - 4.0f, iconSize, iconSize - 1.0f}, theme_manager::get(theme_manager::CONTROL_ICON), 1.5f);
    color closeIcon = hover_close_ ? theme_manager::get(theme_manager::CONTROL_ICON_HOVER) : theme_manager::get(theme_manager::CONTROL_ICON);
    renderer->draw_line({cx[2] - 5.0f, cy - 5.0f}, {cx[2] + 5.0f, cy + 5.0f}, closeIcon, 1.5f);
    renderer->draw_line({cx[2] + 5.0f, cy - 5.0f}, {cx[2] - 5.0f, cy + 5.0f}, closeIcon, 1.5f);
}

} 
