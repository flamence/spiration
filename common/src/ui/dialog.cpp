/**
 * @file dialog.cpp
 * @brief 模态对话框实现。
 * @author clk
 */

#include <ui/dialog.h>
#include <ui/label.h>

namespace spiration {

void dialog::init() {
    widget_style.background_color = theme_manager::get(theme_manager::DIALOG_BG);
}

void dialog::show() { visible_ = true; if (request_repaint_) request_repaint_(); }
void dialog::dismiss() { visible_ = false; if (on_close) on_close(); if (request_repaint_) request_repaint_(); }

void dialog::layout() {
    container::layout();
    if (content_) {
        content_->x = 12.0f;
        content_->y = title_bar_h_;
        content_->width = width - 24.0f;
        content_->height = height - title_bar_h_ - 12.0f;
        content_->layout();
    }
}

void dialog::paint(std::shared_ptr<renderer> renderer) {
    if (!visible_) return;
    float ox = x, oy = y;
    uint32_t vw = 0, vh = 0;
    renderer->get_viewport_size(vw, vh);
    x = 0; y = 0; width = static_cast<float>(vw); height = static_cast<float>(vh);
    renderer->draw_rectangle({x, y, width, height}, theme_manager::get(theme_manager::DIALOG_OVERLAY));
    x = ox; y = oy;
    width = 400.0f; height = 250.0f;
    renderer->draw_rounded_rectangle({x, y, width, height}, theme_manager::get(theme_manager::DIALOG_BG), 8.0f);
    renderer->draw_rounded_rectangle_outline({x, y, width, height}, theme_manager::get(theme_manager::POPUP_BORDER), 8.0f, 1.0f);

    renderer->draw_text_aligned(title, {x + 16.0f, y, width - 16.0f, title_bar_h_},
                                theme_manager::get(theme_manager::LABEL_TEXT),
                                text_alignment::left, vertical_alignment::center, 14.0f);
    renderer->draw_line({x, y + title_bar_h_}, {x + width, y + title_bar_h_}, theme_manager::get(theme_manager::SEPARATOR), 1.0f);

    widget::paint(renderer);
}

void dialog::handle_event(const event_type& type, void* data) {
    if (!visible_) return;
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        md->consumed = true;
    }
    container::handle_event(type, data);
}

void message_dialog::set_message(const std::string& msg) {
    title = msg;
}

void message_dialog::add_button(const std::string& label, std::function<void()> callback) {
    auto btn = std::make_unique<button>();
    btn->text = label;
    btn->init();
    btn->widget_style.width = 80;
    btn->widget_style.height = 28;
    btn->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    btn->on_click = std::move(callback);
    add_child(std::move(btn));
    layout();
}

} // namespace spiration
