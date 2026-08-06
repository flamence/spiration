/**
 * @file collapsible.cpp
 * @brief 可折叠容器控件实现。
 * @author clk
 */

#include <ui/collapsible.h>
#include <ui/container.h>
#include <ui/focus_manager.h>
#include <ui/layout.h>
#include <ui/theme_manager.h>

namespace spiration {

void collapsible::set_content(std::unique_ptr<widget> w) {
    if (!w) return;
    content_ = w.get();
    add_child(std::move(w));
    if (request_repaint_) request_repaint_();
}

float collapsible::content_target_height() const {
    if (!content_) return 0.0f;
    float natural = content_measured_h_;
    if (natural <= 0.0f) {
        size pref = content_->layout_preferred_size();
        natural = (pref.height > 0.0f) ? pref.height : content_->height;
    }
    if (max_content_height > 0.0f) return std::min(natural, max_content_height);
    return natural;
}

void collapsible::set_expanded(bool v) {
    if (expanded == v) return;
    expanded = v;
    content_height_.animate_to(expanded ? 1.0f : 0.0f, animation_ms);
    if (request_repaint_) request_repaint_();
    if (on_toggle) on_toggle(expanded);
    if (expanded) {
        relayout_chain();
    }
}

void collapsible::layout() {
    if (content_) {
        content_->x = 0;
        content_->y = summary_height;
        content_->width = width;
        content_->height = (max_content_height > 0.0f) ? max_content_height : 10000.0f;
        if (content_->needs_layout()) content_->layout();
        if (auto* c = dynamic_cast<container*>(content_)) {
            content_measured_h_ = c->content_height();
        } else {
            content_measured_h_ = content_->height;
        }
    }
    if (expanded || content_height_.is_animating()) {
        height = summary_height + content_target_height();
    } else {
        height = summary_height;
    }
    widget::layout();
}

size collapsible::layout_preferred_size() const {
    float w = width > 0.0f ? width : 240.0f;
    float h = summary_height;
    if (expanded || content_height_.is_animating()) h += content_target_height();
    return {w, h};
}

void collapsible::tick(float dt_ms) {
    bool was_animating = content_height_.is_animating();
    bool changed = content_height_.update(dt_ms);
    if (changed || was_animating) {
        if (request_repaint_) request_repaint_();
        if (!expanded && was_animating && !content_height_.is_animating()) {
            relayout_chain();
        }
    }
    if (content_) content_->tick(dt_ms);
    widget::tick(dt_ms);
}

void collapsible::on_hover_change(bool hovered) {
    if (hovered) {
        summary_bg_.animate_to(theme_manager::get(theme_manager::BUTTON_HOVER), 100.0f);
    } else {
        summary_bg_.animate_to(color::transparent(), 100.0f);
    }
}

void collapsible::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);

        bool in_summary = md->position.x >= 0.0f && md->position.x <= width &&
                          md->position.y >= 0.0f && md->position.y <= summary_height;
        if (in_summary != hovered_summary_) {
            hovered_summary_ = in_summary;
            on_hover_change(in_summary);
        }

        if (md->action == mouse_action::down && md->button == mouse_button::left && in_summary) {
            focus_manager::instance().request_focus(this);
            toggle();
            md->consumed = true;
            return;
        }

        bool show = expanded || content_height_.is_animating();
        if (content_ && show) {
            float ch = content_height_.current() * content_target_height();
            bool in_content = md->position.x >= 0.0f && md->position.x <= width &&
                              md->position.y > summary_height &&
                              md->position.y <= summary_height + ch;
            if (in_content) {
                point orig = md->position;
                md->position.y -= summary_height;
                const bool before = md->consumed;
                content_->handle_event(type, data);
                md->position = orig;
                if (md->consumed && !before) return;
            }
        }
        bool in_bounds = md->position.x >= 0.0f && md->position.x <= width &&
                         md->position.y >= 0.0f && md->position.y <= height;
        if (in_bounds && (md->action == mouse_action::down || md->action == mouse_action::up)) {
            md->consumed = true;
        }
        return;
    }
    if (content_ && (expanded || content_height_.is_animating())) {
        content_->handle_event(type, data);
    }
}

void collapsible::paint(std::shared_ptr<renderer> renderer) {
    const float fs = 13.0f;
    const std::string fam = theme_manager::get_str(theme_manager::UI_FONT);

    renderer->draw_rectangle({0, 0, width, summary_height}, summary_bg_.current());

    std::string marker = expanded ? "\xE2\x96\xBE" : "\xE2\x96\xB8";
    renderer->draw_text(marker, {10.0f, (summary_height - fs) * 0.5f},
                        theme_manager::get(theme_manager::TAB_INACTIVE_TEXT), fs, fam, false);
    renderer->draw_text(summary_text, {26.0f, (summary_height - fs) * 0.5f},
                        theme_manager::get(theme_manager::LABEL_TEXT), fs, fam, false);

    if (content_) {
        bool show = expanded || content_height_.is_animating();
        if (show) {
            float ch = content_height_.current() * content_target_height();
            if (ch > 0.5f) {
                renderer->push_clip({0, summary_height, width, ch});
                renderer->push_transform(0, summary_height);
                content_->paint(renderer);
                renderer->pop_transform();
                renderer->pop_clip();
            }
        }
    }
}

} // namespace spiration
