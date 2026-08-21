/**
 * @file collapsible.cpp
 * @brief 可折叠容器控件实现。
 * @author clk
 */

#include <cmath>
#include <spiration/ui/collapsible.h>
#include <spiration/ui/container.h>
#include <spiration/ui/focus_manager.h>
#include <spiration/ui/layout.h>
#include <spiration/ui/theme_manager.h>
#include <string>

namespace spiration {

void collapsible::set_content(std::unique_ptr<widget> w) {
    if (!w) return;
    content_ = w.get();
    add_child(std::move(w));
    // 内容更换后目标高度需重新按实测值初始化，避免沿用旧内容的目标动画。
    target_h_measured_ = false;
    if (request_repaint_) request_repaint_();
}

float collapsible::content_target_height() const {
    if (!content_) return 0.0f;
    float natural = content_measured_h_;
    // 仅当从未测量（-1）时回退到首选尺寸；内容真实为空（测得 0）时目标为 0，
    // 避免空内容回退到容器的 max_content_height 使胶囊无法收起。
    if (natural < 0.0f) {
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
    // 切换由 content_height_ 平滑驱动；layout() 的切换分支会把 height_anim_
    // 逐帧同步到实际高度，此处无需预置目标（避免陈旧测量值导致跳变）。
    if (request_repaint_) request_repaint_();
    if (on_toggle) on_toggle(expanded);
    // 立即按当前过渡值重排一次（有界深度），动画期间由 tick 每帧跟随。
    relayout_chain(relayout_depth);
}

void collapsible::layout() {
    on_layout_begin();
    // 折叠动画期间冻结内容测量与布局：收起动画基于展开时的内容高度平滑进行，
    // 清空内容时胶囊高度仍从原值平滑缩到摘要栏（而非瞬间跳变）。
    const bool folding = content_height_.is_animating() && !expanded;
    if (content_ && !folding) {
        content_->x = 0;
        content_->y = summary_height;
        content_->width = width;
        content_->height = (max_content_height > 0.0f) ? max_content_height : 10000.0f;
        content_->layout();
        if (auto* c = dynamic_cast<container*>(content_)) {
            content_measured_h_ = c->content_height();
        } else {
            content_measured_h_ = content_->height;
        }
    }
    const float raw_target = content_target_height();
    float target_h = raw_target;
    if (content_height_.is_animating()) {
        // 切换动画期间内容可能仍在增长（思考/工具流式输出、列表追加）：
        // 目标高度平滑追向实测值，避免 height = progress * target 中 target
        // 在内容块到达时跳变（todo 列表稳定内容平滑、流式内容则卡顿的根因）。
        if (!target_h_measured_) {
            target_h_anim_.snap_to(raw_target);
            target_h_measured_ = true;
        } else if (std::fabs(target_h_anim_.current() - raw_target) > 0.5f) {
            target_h_anim_.animate_to(raw_target, std::min(140.0f, animation_ms));
        } else {
            target_h_anim_.snap_to(raw_target);
        }
        target_h = target_h_anim_.current();
    } else {
        // 非切换期间与实测目标保持同步，切换开始时无陈旧值跳变。
        target_h_anim_.snap_to(raw_target);
    }
    const float desired = summary_height + content_height_.current() * target_h;
    if (content_height_.is_animating()) {
        // 切换动画期间：高度直接跟随 content_height_（已平滑）。
        height_anim_.snap_to(desired);
        height = desired;
    } else {
        // 静止/流式增长：高度平滑追向目标，避免内容增长时跳变。
        if (std::fabs(height_anim_.current() - desired) > 0.5f) {
            height_anim_.animate_to(desired, std::min(140.0f, animation_ms));
        } else {
            height_anim_.snap_to(desired);
        }
        height = height_anim_.current();
    }
    // 内容控件绘制高度跟随胶囊当前可见内容高度（而非其完整高度）：
    // 内容增减时由胶囊动画同步裁剪（增长从顶部露出、减少从底部裁掉），
    // 与 paint 的裁剪区（height - summary）一致，避免内容高度与动画高度
    // 不一致造成下方空白或内容溢出。
    if (content_) {
        content_->height = std::max(0.0f, height - summary_height);
    }
}

size collapsible::layout_preferred_size() const {
    float w = width > 0.0f ? width : 240.0f;
    float h = height_anim_.current();
    if (h <= 0.0f) h = summary_height;
    if (content_height_.is_animating())
        h = summary_height + content_target_height();
    return {w, h};
}

void collapsible::tick(float dt_ms) {
    const bool was_a = content_height_.is_animating();
    const bool a_changed = content_height_.update(dt_ms);
    const bool was_h = height_anim_.is_animating();
    const bool h_changed = height_anim_.update(dt_ms);
    const bool was_t = target_h_anim_.is_animating();
    const bool t_changed = target_h_anim_.update(dt_ms);
    if (a_changed || was_a || h_changed || was_h || t_changed || was_t) {
        if (request_repaint_) request_repaint_();
        if (content_height_.is_animating() || height_anim_.is_animating() ||
            target_h_anim_.is_animating()) {
            // 动画期间只重排到最近的手动布局宿主，避免每帧全链根布局。
            relayout_chain(relayout_depth);
        } else if (a_changed || was_a) {
            // 切换动画结束：全链重排一次收尾（滚动范围、上层布局等）。
            // 高度追逐（流式增长）结束不触发全链，避免反复全量布局造成卡顿。
            relayout_chain();
            // 折叠动画完成：通知外部收起（如清空待办后隐藏胶囊）。
            if (!expanded && on_collapsed) on_collapsed();
        }
    }
    // 内容由 widget::tick 的子控件循环统一驱动，不再单独 tick（避免每帧双次）。
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

        // 摘要栏右侧操作按钮（悬停/点击优先级高于折叠切换）。
        if (show_summary_action && in_summary) {
            const rectangle ar = action_rect();
            const bool over_action =
                md->position.x >= ar.x && md->position.x <= ar.x + ar.width &&
                md->position.y >= ar.y && md->position.y <= ar.y + ar.height;
            if (over_action != hover_action_) {
                hover_action_ = over_action;
                if (request_repaint_) request_repaint_();
            }
            if (md->action == mouse_action::down && md->button == mouse_button::left &&
                over_action) {
                md->consumed = true;
                if (on_summary_action) on_summary_action();
                return;
            }
        } else if (hover_action_) {
            hover_action_ = false;
            if (request_repaint_) request_repaint_();
        }

        if (md->action == mouse_action::down && md->button == mouse_button::left && in_summary) {
            focus_manager::instance().request_focus(this);
            toggle();
            md->consumed = true;
            return;
        }

        bool show = expanded || content_height_.is_animating();
        if (content_ && show) {
            const float ch = std::max(0.0f, height - summary_height);
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

    float text_w = renderer->measure_text_width(summary_text, fs, fam);
    float max_w = width - 34.0f;
    if (show_summary_action) max_w -= 30.0f;
    std::string shown = summary_text;
    if (text_w > max_w && max_w > 0.0f) {
        // 简单截断显示，避免顶到右侧操作按钮。
        while (!shown.empty() && renderer->measure_text_width(shown, fs, fam) > max_w) {
            shown.resize(shown.size() - 1);
        }
    }
    renderer->draw_text(shown, {26.0f, (summary_height - fs) * 0.5f},
                        theme_manager::get(theme_manager::LABEL_TEXT), fs, fam, false);

    if (show_summary_action) {
        const rectangle ar = action_rect();
        if (hover_action_) {
            renderer->draw_rectangle(ar, theme_manager::get(theme_manager::LIST_ITEM_HOVER));
        }
        const float cx = ar.x + ar.width * 0.5f;
        const float cy = ar.y + ar.height * 0.5f;
        const float s = 4.5f;
        const color c = theme_manager::get(theme_manager::TEXT_MUTED);
        renderer->draw_line({cx - s, cy - s}, {cx + s, cy + s}, c, 1.5f);
        renderer->draw_line({cx + s, cy - s}, {cx - s, cy + s}, c, 1.5f);
    }

    if (content_) {
        bool show = expanded || content_height_.is_animating();
        if (show) {
            // 内容裁剪高度 = 动画当前可见内容区（用 height_anim_ 而非 height：
            // layout 写入的 height 可能滞后于动画，而 height_anim_ 由 tick 每帧
            // 更新，保证裁剪区与动画进度严格一致）。
            const float ch = std::max(0.0f, height_anim_.current() - summary_height);
            if (ch > 0.5f) {
                // 内容随收起进度透明过渡。
                renderer->set_alpha(content_height_.current());
                renderer->push_clip({0, summary_height, width, ch});
                renderer->push_transform(0, summary_height);
                content_->paint(renderer);
                renderer->pop_transform();
                renderer->pop_clip();
                renderer->set_alpha(1.0f);
            }
        }
    }
}

} // namespace spiration
