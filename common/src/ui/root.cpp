#include <ui/root.h>
#include <ui/appbar.h>
#include <ui/cursor.h>
#include <ui/focus_manager.h>
#include <ui/layout.h>
#include <ui/menu_bar.h>
#include <ui/theme_manager.h>

namespace spiration {

namespace {

/// @brief 把屏幕坐标转换为 w 的局部坐标。
point root_local(widget* w, const widget* root, const point& screen) {
    float ox = 0.0f, oy = 0.0f;
    bool first = true;
    for (widget* p = w; p && p != root; p = p->parent()) {
        if (first) {
            ox += p->x;
            oy += p->y;
            first = false;
        } else {
            ox += p->x - p->scroll_offset_x_for_children();
            oy += p->y - p->scroll_offset_for_children();
        }
    }
    return {screen.x - ox, screen.y - oy};
}

} // namespace

root::root(std::shared_ptr<spiration::window> parent, bool create_appbar) {
    m_window = parent;
    create_appbar_ = create_appbar;
    widget_style.background_color = theme_manager::get(theme_manager::WINDOW_BG);
    cursor_manager::instance().set_window(m_window.get());
    init();
}

void root::add_overlay(widget* w) {
    if (m_overlay && m_overlay != w) {
        m_overlay->set_focused(false);
    }
    m_overlay = w;
}

void root::remove_overlay(widget* w) {
    if (m_overlay == w) m_overlay = nullptr;
}

void root::paint(std::shared_ptr<renderer> renderer) {
    container::paint(renderer);
    if (m_popup) {
        renderer->push_transform(m_popup->x, m_popup->y);
        m_popup->paint(renderer);
        renderer->pop_transform();
    }
    if (m_context_menu && m_context_menu->visible()) {
        renderer->push_transform(m_context_menu->x, m_context_menu->y);
        m_context_menu->paint(renderer);
        renderer->pop_transform();
    }
}

void root::set_mouse_capture(widget* w) {
    captured_ = w;
    if (m_window) m_window->set_mouse_capture(w != nullptr);
}

void root::notify_selection_started(widget* w) {
    if (selecting_widget_ && selecting_widget_ != w) {
        selecting_widget_->clear_text_selection();
    }
    selecting_widget_ = w;
}

void root::on_widget_destroyed(widget* w) {
    if (captured_ == w) {
        captured_ = nullptr;
        if (m_window) m_window->set_mouse_capture(false);
    }
    if (selecting_widget_ == w) {
        selecting_widget_ = nullptr;
    }
    if (m_overlay == w) {
        m_overlay = nullptr;
    }
}

widget* root::hit_test_hover(float x, float y) const {
    if (m_context_menu && m_context_menu->visible()) {
        if (widget* h = m_context_menu->hit_test_hover(x - m_context_menu->x,
                                                      y - m_context_menu->y)) {
            return h;
        }
    }
    if (m_popup) {
        if (widget* h = m_popup->hit_test_hover(x - m_popup->x, y - m_popup->y)) {
            return h;
        }
    }
    if (m_overlay) {
        point lp = root_local(m_overlay, this, {x, y});
        if (widget* h = m_overlay->hit_test_hover(lp.x, lp.y)) {
            return h;
        }
    }
    return container::hit_test_hover(x, y);
}

void root::handle_event(const event_type& type, void* data) {
    if (type == event_type::window_resize) {
        layout();
    }

    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        cursor_manager::instance().update(this, md->position.x, md->position.y);
    }

    if (m_context_menu && m_context_menu->visible() && type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        point old = md->position;
        md->position.x = old.x - m_context_menu->x;
        md->position.y = old.y - m_context_menu->y;
        m_context_menu->handle_event(type, data);
        md->position = old;
        if (md->consumed) return;
    }

    if (m_popup && type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        point old = md->position;
        md->position.x = old.x - m_popup->x;
        md->position.y = old.y - m_popup->y;
        m_popup->handle_event(type, data);
        md->position = old;
        if (md->consumed) return;
    }

    if (m_overlay && type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        point lp = root_local(m_overlay, this, md->position);
        point old = md->position;
        md->position = lp;
        m_overlay->handle_event(type, data);
        md->position = old;
        if (md->consumed) return;
    }

    if (type == event_type::mouse && captured_) {
        auto* md = static_cast<mouse_event_data*>(data);
        point original = md->position;
        float ox = 0.0f, oy = 0.0f;
        bool first = true;
        for (widget* w = captured_; w && w != this; w = w->parent()) {
            if (first) {
                ox += w->x;
                oy += w->y;
                first = false;
            } else {
                ox += w->x - w->scroll_offset_x_for_children();
                oy += w->y - w->scroll_offset_for_children();
            }
        }
        md->position.x = original.x - ox;
        md->position.y = original.y - oy;
        captured_->handle_event(type, data);
        md->position = original;
        return;
    }

    container::handle_event(type, data);

    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        if (md->action == mouse_action::down && !md->consumed) {
            focus_manager::instance().clear_focus();
        }
    }
}

void root::tick(float dt_ms) {
    container::tick(dt_ms);
    if (m_popup) m_popup->tick(dt_ms);
    if (m_context_menu) m_context_menu->tick(dt_ms);
}

void root::layout() {
    int32_t w, h;
    m_window->get_size(w, h);
    width = static_cast<float>(w);
    height = static_cast<float>(h);

    bool has_appbar = false;
    for (auto& child : children()) {
        if (dynamic_cast<appbar*>(child.get())) {
            child->width = width; child->height = 34.0f;
            child->x = 0; child->y = 0;
            child->layout();
            has_appbar = true;
        }
    }

    for (auto& child : children()) {
        if (auto* tb = dynamic_cast<tab_bar*>(child.get())) {
            if (has_appbar) {
                tb->width = width; tb->height = height - 34.0f;
                tb->x = 0; tb->y = 34.0f;
            } else {
                tb->width = width; tb->height = height;
                tb->x = 0; tb->y = 0;
            }
            tb->layout();
        }
    }

    if (m_popup) m_popup->layout();
    if (m_context_menu) m_context_menu->layout();
}

void root::init() {
    auto tb = std::make_unique<tab_bar>();
    tb->init();
    tb->set_repaint_callback(request_repaint_);
    tb->set_window_action_callback(window_action_);
    tab_bar_ = tb.get();
    add_child(std::move(tb));

    if (create_appbar_) {
        auto appbar = std::make_unique<spiration::appbar>();
        appbar->init();
        for (auto& child : appbar->children()) {
            if (auto* mb = dynamic_cast<menu_bar*>(child.get())) {
                menu_bar_ = mb;
                mb->set_show_popup_callback([this](float x, float y, std::unique_ptr<popup_menu> popup) {
                    show_popup(x, y, std::move(popup));
                });
            }
        }
        add_child(std::move(appbar));
    }

    set_context_menu_callback([this](float x, float y, std::unique_ptr<context_menu> menu) {
        show_context_menu(x, y, std::move(menu));
    });
}

bool root::add_menu_item(const std::string& menu_name,
                          const std::string& label,
                          std::function<void()> callback) {
    if (!menu_bar_) return false;
    return menu_bar_->add_sub_item(menu_name, label, std::move(callback));
}

void root::open_tab(std::unique_ptr<tab> t) {
    if (!tab_bar_ || !t) return;
    tab_bar_->add_tab(std::move(t));
}

void root::show_popup(float x, float y, std::unique_ptr<popup_menu> popup) {
    dismiss_popup();
    popup->x = x;
    popup->y = y;
    popup->set_dismiss_callback([this]() { dismiss_popup(); });
    popup->init();
    
    if (request_repaint_) popup->set_repaint_callback(request_repaint_);
    if (window_action_) popup->set_window_action_callback(window_action_);
    m_popup = std::move(popup);
    if (request_repaint_) request_repaint_();
}

void root::dismiss_popup() {
    if (m_popup) {
        m_popup.reset();
        if (request_repaint_) request_repaint_();
    }
}

void root::show_context_menu(float x, float y, std::unique_ptr<context_menu> menu) {
    if (m_context_menu) m_context_menu.reset();
    if (!menu) return;
    if (x + menu->width > width) x = std::max(0.0f, width - menu->width);
    if (y + menu->height > height) y = std::max(0.0f, height - menu->height);
    menu->set_repaint_callback(request_repaint_);
    menu->set_window_action_callback(window_action_);
    menu->on_dismiss = [this]() { m_context_menu.reset(); };
    menu->show_at(x, y);
    m_context_menu = std::move(menu);
    if (request_repaint_) request_repaint_();
}

void root::dismiss_context_menu() {
    if (m_context_menu) {
        m_context_menu->on_dismiss = nullptr;
        m_context_menu.reset();
        if (request_repaint_) request_repaint_();
    }
}

} // namespace spiration