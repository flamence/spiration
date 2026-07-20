#include <ui/root.h>
#include <ui/appbar.h>
#include <ui/layout.h>
#include <ui/menu_bar.h>
#include <ui/theme_manager.h>

namespace spiration {

root::root(std::shared_ptr<spiration::window> parent, bool create_appbar) {
    m_window = parent;
    create_appbar_ = create_appbar;
    widget_style.background_color = theme_manager::get(theme_manager::WINDOW_BG);
    init();
}

void root::paint(std::shared_ptr<renderer> renderer) {
    container::paint(renderer);
    if (m_popup) {
        m_popup->paint(renderer);
    }
}

void root::set_mouse_capture(widget* w) {
    captured_ = w;
    if (m_window) m_window->set_mouse_capture(w != nullptr);
}

void root::handle_event(const event_type& type, void* data) {
    if (type == event_type::window_resize) {
        layout();
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

    if (type == event_type::mouse && captured_) {
        auto* md = static_cast<mouse_event_data*>(data);
        point original = md->position;
        float ox = 0.0f, oy = 0.0f;
        for (widget* w = captured_; w && w != this; w = w->parent()) {
            ox += w->x;
            oy += w->y;
        }
        md->position.x = original.x - ox;
        md->position.y = original.y - oy;
        captured_->handle_event(type, data);
        md->position = original;
        return;
    }

    container::handle_event(type, data);
}

void root::tick(float dt_ms) {
    container::tick(dt_ms);
    if (m_popup) m_popup->tick(dt_ms);
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

}