/**
 * @file todo_view.cpp
 * @brief 待办事项可视化控件实现。
 * @author clk
 */

#include <extension/builtin/agent/ui/todo_view.h>
#include <ui/focus_manager.h>
#include <ui/theme_manager.h>

#include <cstdint>

namespace spiration {
namespace agent {

namespace {

constexpr float PAD        = 8.0f;
constexpr float ROW_H      = 24.0f;
constexpr float MARKER_SZ  = 14.0f;
constexpr float FONT       = 13.0f;

} // namespace

todo_view::todo_view() {
    widget_style.background_color = color::transparent();
    focusable = true;
}

float todo_view::desired_height() const {
    if (items_.empty()) return 0.0f;
    return PAD + ROW_H * static_cast<float>(items_.size()) + PAD;
}

void todo_view::refresh() {
    items_ = todo_store::instance().items();
    hovered_row_ = -1;
    height = std::min(desired_height(), max_height);
    scroll_y_ = std::max(0.0f, std::min(scroll_y_, scroll_max_y()));
    if (request_repaint_) request_repaint_();
    if (on_content_changed) on_content_changed();
}

void todo_view::tick(float dt_ms) {
    uint64_t ver = todo_store::instance().version();
    if (ver != seen_version_) {
        seen_version_ = ver;
        refresh();
    }
    widget::tick(dt_ms);
}

void todo_view::layout() {
    height = std::min(desired_height(), max_height);
    scroll_y_ = std::max(0.0f, std::min(scroll_y_, scroll_max_y()));
    widget::layout();
}

size todo_view::layout_preferred_size() const {
    float w = width > 0.0f ? width : 240.0f;
    return {w, std::min(desired_height(), max_height)};
}

void todo_view::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        float mx = md->position.x;
        float my = md->position.y;

        if (md->action == mouse_action::wheel) {
            float smax = scroll_max_y();
            if (mx >= 0.0f && mx <= width && my >= 0.0f && my <= height && smax > 0.0f) {
                float step = (md->wheel_delta > 0) ? -ROW_H : ROW_H;
                float ny = std::max(0.0f, std::min(scroll_y_ + step, smax));
                if (ny != scroll_y_) {
                    scroll_y_ = ny;
                    md->consumed = true;
                    if (request_repaint_) request_repaint_();
                    return;
                }
            }
            widget::handle_event(type, data);
            return;
        }

        const float cy = my + scroll_y_;

        if (md->action == mouse_action::move) {
            int row = -1;
            if (mx >= 0.0f && mx <= width && cy >= PAD &&
                cy < PAD + ROW_H * static_cast<float>(items_.size())) {
                row = static_cast<int>((cy - PAD) / ROW_H);
                if (row < 0 || row >= static_cast<int>(items_.size())) row = -1;
            }
            if (row != hovered_row_) {
                hovered_row_ = row;
                if (request_repaint_) request_repaint_();
            }
            widget::handle_event(type, data);
            return;
        }

        if (md->action == mouse_action::down && md->button == mouse_button::left) {
            if (mx >= 0.0f && mx <= width && cy >= PAD) {
                int row = static_cast<int>((cy - PAD) / ROW_H);
                if (row >= 0 && row < static_cast<int>(items_.size())) {
                    md->consumed = true;
                    focus_manager::instance().request_focus(this);
                    auto items = todo_store::instance().items();
                    if (row < static_cast<int>(items.size())) {
                        switch (items[row].status) {
                            case todo_status::pending:    items[row].status = todo_status::in_progress; break;
                            case todo_status::in_progress: items[row].status = todo_status::completed;  break;
                            case todo_status::completed:   items[row].status = todo_status::pending;    break;
                        }
                        todo_store::instance().set(items);
                    }
                    if (request_repaint_) request_repaint_();
                    return;
                }
            }
        }
    }
    widget::handle_event(type, data);
}

void todo_view::paint(std::shared_ptr<renderer> renderer) {
    if (items_.empty()) return;

    const std::string fam = theme_manager::get_str(theme_manager::UI_FONT);
    const color text   = theme_manager::get(theme_manager::LABEL_TEXT);
    const color muted  = theme_manager::get(theme_manager::TEXT_MUTED);
    const color border = theme_manager::get(theme_manager::CHECKBOX_BORDER);
    const color check  = theme_manager::get(theme_manager::CHECKBOX_CHECK_BG);
    const color prog   = theme_manager::get(theme_manager::PROGRESS_FILL);
    const color hover  = theme_manager::get(theme_manager::LIST_ITEM_HOVER);

    renderer->push_clip({0, 0, width, height});
    renderer->push_transform(0, -scroll_y_);
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        float y = PAD + ROW_H * static_cast<float>(i);
        float cy = y + (ROW_H - MARKER_SZ) * 0.5f;
        const rectangle box{PAD, cy, MARKER_SZ, MARKER_SZ};

        if (static_cast<int>(i) == hovered_row_) {
            renderer->draw_rectangle({PAD, y, width - PAD * 2.0f, ROW_H}, hover);
        }

        switch (item.status) {
            case todo_status::pending:
                renderer->draw_rectangle_outline(box, border, 1.2f);
                break;
            case todo_status::in_progress:
                renderer->draw_rounded_rectangle(box, prog, 3.0f);
                break;
            case todo_status::completed:
                renderer->draw_rounded_rectangle(box, check, 3.0f);
                renderer->draw_line({box.x + 3.0f, cy + MARKER_SZ * 0.5f},
                                    {box.x + MARKER_SZ * 0.45f, box.y + MARKER_SZ - 4.0f},
                                    theme_manager::get(theme_manager::CHECKBOX_CHECK_FG), 1.6f);
                renderer->draw_line({box.x + MARKER_SZ * 0.45f, box.y + MARKER_SZ - 4.0f},
                                    {box.x + MARKER_SZ - 2.0f, cy + 3.0f},
                                    theme_manager::get(theme_manager::CHECKBOX_CHECK_FG), 1.6f);
                break;
        }

        float text_y = y + (ROW_H - FONT) * 0.5f;
        renderer->draw_text(item.content, {PAD + MARKER_SZ + 8.0f, text_y},
                            item.status == todo_status::completed ? muted : text,
                            FONT, fam, false);
    }
    renderer->pop_transform();
    renderer->pop_clip();

    if (focused_) {
        renderer->draw_rectangle_outline({0, 0, width, height},
                                         theme_manager::get(theme_manager::INPUT_FOCUS_BORDER), 1.0f);
    }
}

} // namespace agent
} // namespace spiration
