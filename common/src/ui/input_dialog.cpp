/**
 * @file input_dialog.cpp
 * @brief 模态输入对话框实现。
 * @author clk
 */

#include <ui/input_dialog.h>
#include <ui/label.h>
#include <ui/layout.h>
#include <ui/theme_manager.h>
#include <extension/builtin/i18n/i18n.h>

namespace spiration {

input_dialog::input_dialog() {
    widget_style.background_color = color::transparent();

    auto f = std::make_unique<text_field>();
    f->font_size = 13.0f;
    f->on_submit = [this](const std::string&) { confirm(); };
    field_ = f.get();
    add_child(std::move(f));

    auto ok = std::make_unique<button>();
    ok->text = i18n_manager::get().tr("dialog.ok");
    ok->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    ok->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    ok->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    ok->on_click = [this]() { confirm(); };
    ok_ = ok.get();
    add_child(std::move(ok));

    auto cancel_btn = std::make_unique<button>();
    cancel_btn->text = i18n_manager::get().tr("dialog.cancel");
    cancel_btn->hover_color = theme_manager::get(theme_manager::CLOSE_HOVER);
    cancel_btn->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    cancel_btn->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    cancel_btn->on_click = [this]() { cancel(); };
    cancel_ = cancel_btn.get();
    add_child(std::move(cancel_btn));
}

void input_dialog::recalc_geometry() {
    const float bw = 420.0f, bh = 160.0f;
    const float bx = (width - bw) * 0.5f, by = (height - bh) * 0.5f;
    box_rect_ = {bx, by, bw, bh};
    field_rect_ = {bx + 16, by + 46, bw - 32, 30};
    ok_rect_ = {bx + bw - 16 - 80 - 8 - 80, by + bh - 16 - 32, 80, 32};
    cancel_rect_ = {bx + bw - 16 - 80, by + bh - 16 - 32, 80, 32};

    if (field_) {
        field_->x = field_rect_.x; field_->y = field_rect_.y;
        field_->width = field_rect_.width; field_->height = field_rect_.height;
        field_->widget_style.height = field_rect_.height;
    }
    if (ok_) {
        ok_->x = ok_rect_.x; ok_->y = ok_rect_.y;
        ok_->width = ok_rect_.width; ok_->height = ok_rect_.height;
    }
    if (cancel_) {
        cancel_->x = cancel_rect_.x; cancel_->y = cancel_rect_.y;
        cancel_->width = cancel_rect_.width; cancel_->height = cancel_rect_.height;
    }
}

void input_dialog::layout() {
    recalc_geometry();
    container::layout();
}

void input_dialog::show(const std::string& title, const std::string& placeholder,
                        const std::string& initial,
                        std::function<void(const std::string&)> on_confirm,
                        std::function<void()> on_cancel) {
    title_ = title;
    on_confirm_ = std::move(on_confirm);
    on_cancel_ = std::move(on_cancel);
    if (field_) {
        field_->placeholder = placeholder;
        field_->text = initial;
        field_->select_all();
        field_->focus();
    }
    visible_ = true;
    fade_.animate_to(1.0f, 150.0f);
    recalc_geometry();
    if (request_repaint_) request_repaint_();
}

void input_dialog::dismiss() {
    visible_ = false;
    if (field_) field_->blur();
    fade_.animate_to(0.0f, 150.0f);
    if (request_repaint_) request_repaint_();
}

void input_dialog::confirm() {
    std::string txt = field_ ? field_->text : "";
    auto cb = on_confirm_;
    dismiss();
    if (cb) cb(txt);
}

void input_dialog::cancel() {
    auto cb = on_cancel_;
    dismiss();
    if (cb) cb();
}

void input_dialog::tick(float dt_ms) {
    if (fade_.update(dt_ms) && request_repaint_) request_repaint_();
    container::tick(dt_ms);
}

void input_dialog::paint(std::shared_ptr<renderer> renderer) {
    float fade = fade_.current();
    if (fade <= 0.01f) return;

    const std::string fam = theme_manager::get_str(theme_manager::UI_FONT);
    renderer->draw_rectangle({0, 0, width, height}, {0.0f, 0.0f, 0.0f, 0.5f * fade});

    color box_bg = theme_manager::get(theme_manager::DIALOG_BG);
    box_bg.a *= fade;
    renderer->draw_rounded_rectangle(box_rect_, box_bg, 8.0f);
    renderer->draw_rounded_rectangle_outline(box_rect_,
                                             theme_manager::get(theme_manager::POPUP_BORDER),
                                             8.0f, 1.0f);

    color title_c = theme_manager::get(theme_manager::LABEL_TEXT);
    title_c.a *= fade;
    renderer->draw_text(title_, {box_rect_.x + 16, box_rect_.y + 14},
                        title_c, 15.0f, fam, false);

    if (visible_) container::paint(renderer);
}

void input_dialog::handle_event(const event_type& type, void* data) {
    if (!visible_) return;
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x, my = md->position.y;
        auto in_rect = [&](const rectangle& r) {
            return mx >= r.x && mx <= r.x + r.width && my >= r.y && my <= r.y + r.height;
        };

        if (md->action == mouse_action::down) {
            if (in_rect(ok_rect_)) { md->consumed = true; confirm(); return; }
            if (in_rect(cancel_rect_)) { md->consumed = true; cancel(); return; }
            if (in_rect(field_rect_)) {
                md->position = {mx - field_rect_.x, my - field_rect_.y};
                field_->handle_event(type, data);
                md->position = {mx, my};
                return;
            }
            md->consumed = true;
            return;
        }
        if (md->action == mouse_action::move) {
            widget* target = nullptr;
            point local;
            if (in_rect(ok_rect_)) { target = ok_; local = {mx - ok_rect_.x, my - ok_rect_.y}; }
            else if (in_rect(cancel_rect_)) { target = cancel_; local = {mx - cancel_rect_.x, my - cancel_rect_.y}; }
            else if (in_rect(field_rect_)) { target = field_; local = {mx - field_rect_.x, my - field_rect_.y}; }
            if (target) {
                md->position = local;
                target->handle_event(type, data);
                md->position = {mx, my};
            }
            md->consumed = true;
            return;
        }
        if (md->action == mouse_action::up) {
            if (field_) {
                md->position = {mx - field_rect_.x, my - field_rect_.y};
                field_->handle_event(type, data);
                md->position = {mx, my};
            }
            md->consumed = true;
            return;
        }
        md->consumed = true;
        return;
    }
    if (type == event_type::keyboard) {
        auto* kd = static_cast<key_event_data*>(data);
        if (kd->key_code == 0x1B) {
            kd->consumed = true;
            cancel();
            return;
        }
        field_->handle_event(type, data);
        kd->consumed = true;
        return;
    }
}

} // namespace spiration
