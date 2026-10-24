#include <spiration/application.h>
#include <spiration/ui/root.h>
#include <spiration/ui/label.h>
#include <spiration/utils/dispatcher.h>
#include <spiration/ui/appbar.h>
#include <spiration/ui/cursor.h>
#include <spiration/ui/focus_manager.h>
#include <spiration/ui/layout.h>
#include <spiration/ui/menu_bar.h>
#include <spiration/ui/theme_manager.h>
#ifdef __APPLE__
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/window/macos_menu.h>
#include <spiration/window/macos_menu_state.h>
#include <fmt/format.h>
#endif

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
    // 开启新一轮绘制：轮次号递增后，上一帧下发的裁剪上下文自动失效，
    // 避免自定义 paint() 直接绘制子控件时误用陈旧裁剪区。
    widget::begin_paint_pass();

    container::paint(renderer);
    // 浮层不属于内容坐标体系，绘制前清除裁剪上下文（其子树退化为仅按自身矩形剔除）。
    if (m_popup) {
        m_popup->clear_paint_clip();
        renderer->push_transform(m_popup->x, m_popup->y);
        m_popup->paint(renderer);
        renderer->pop_transform();
    }
    if (m_context_menu && m_context_menu->visible()) {
        m_context_menu->clear_paint_clip();
        renderer->push_transform(m_context_menu->x, m_context_menu->y);
        m_context_menu->paint(renderer);
        renderer->pop_transform();
    }
    // 覆盖层（展开的下拉框等）最后绘制，保证不被后续兄弟控件遮挡。
    if (m_overlay) {
        m_overlay->clear_paint_clip();
        point lp = m_overlay->to_screen(0.0f, 0.0f);
        renderer->push_transform(lp.x, lp.y);
        m_overlay->paint(renderer);
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

void root::begin_cross_selection(widget* w, size_t offset) {
    clear_all_selections_except(w);
    selecting_widget_ = w;
    selection_cur_widget_ = w;
    selection_anchor_ = offset;
    selection_pos_ = offset;
    selection_dragging_ = true;
}

void root::update_cross_selection(widget* from, float mx, float my) {
    if (!selection_dragging_ || !selecting_widget_) return;
    const point rp = from->to_screen(mx, my);

    label* cur = nearest_selectable_label(hit_test_hover(rp.x, rp.y));
    if (!cur) cur = dynamic_cast<label*>(selecting_widget_);
    if (!cur) return;

    const point lp = cur->to_screen(0.0f, 0.0f);
    selection_cur_widget_ = cur;
    selection_pos_ = cur->text_offset_at(rp.x - lp.x, rp.y - lp.y);
    apply_cross_selection();
}

void root::end_cross_selection(widget* w) {
    (void)w;
    selection_dragging_ = false;
}

label* root::nearest_selectable_label(widget* w) const {
    while (w) {
        label* l = dynamic_cast<label*>(w);
        if (l && l->selectable) return l;
        w = w->parent();
    }
    return nullptr;
}

void root::clear_all_selections_except(widget* keep) {
    std::vector<widget*> stack;
    stack.push_back(this);
    while (!stack.empty()) {
        widget* w = stack.back();
        stack.pop_back();
        if (w != keep) {
            if (auto* l = dynamic_cast<label*>(w)) l->clear_selection();
        }
        for (const auto& ch : w->children()) stack.push_back(ch.get());
    }
}

void root::apply_cross_selection() {
    if (!selecting_widget_ || !selection_cur_widget_) return;
    label* anchor = dynamic_cast<label*>(selecting_widget_);
    label* cur = dynamic_cast<label*>(selection_cur_widget_);
    if (!anchor || !cur) return;

    if (anchor == cur) {
        anchor->set_selection_range(selection_anchor_, selection_pos_);
        return;
    }

    const point a = anchor->to_screen(0.0f, 0.0f);
    const point c = cur->to_screen(0.0f, 0.0f);
    const bool anchor_first = (a.y < c.y) || (a.y == c.y && a.x <= c.x);
    const float y0 = std::min(a.y, c.y);
    const float y1 = std::max(a.y, c.y);

    if (anchor_first)
        anchor->set_selection_range(selection_anchor_, anchor->text.size());
    else
        anchor->set_selection_range(0, selection_anchor_);

    if (anchor_first)
        cur->set_selection_range(0, selection_pos_);
    else
        cur->set_selection_range(selection_pos_, cur->text.size());

    // 位于两者之间的可选文本控件整体选中。
    std::vector<widget*> stack;
    stack.push_back(this);
    while (!stack.empty()) {
        widget* w = stack.back();
        stack.pop_back();
        for (const auto& ch : w->children()) {
            widget* child = ch.get();
            if (child == anchor || child == cur) continue;
            if (auto* l = dynamic_cast<label*>(child)) {
                if (l->selectable) {
                    const point p = l->to_screen(0.0f, 0.0f);
                    if (p.y > y0 && p.y < y1)
                        l->set_selection_range(0, l->text.size());
                }
            }
            stack.push_back(child);
        }
    }
}

void root::on_widget_destroyed(widget* w) {
    if (captured_ == w) {
        captured_ = nullptr;
        if (m_window) m_window->set_mouse_capture(false);
    }
    if (selecting_widget_ == w) {
        selecting_widget_ = nullptr;
        selection_dragging_ = false;
    }
    if (selection_cur_widget_ == w) {
        selection_cur_widget_ = nullptr;
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
        // 立即布局一次，保证 resize 过程中控件（如窗口控制按钮）实时跟随；
        // 平台不再置 needs-layout 标志，避免帧循环二次全量布局。
        layout();
        if (m_window) m_window->request_repaint();
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

    if (type == event_type::keyboard) {
        auto* kd = static_cast<key_event_data*>(data);
        // 标签快捷键在焦点控件之后兜底处理：输入框不消费 Ctrl+W/Ctrl+Tab，
        // 模态对话框会消费全部按键，因此不会误关标签。
        if (!kd->consumed && tab_bar_ && tab_bar_->handle_shortcut(*kd)) {
            kd->consumed = true;
            // 浮层可能引用即将随标签销毁的控件，先关闭。
            dismiss_popup();
            dismiss_context_menu();
        }
    }

    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        if (md->action == mouse_action::down && !md->consumed) {
            focus_manager::instance().clear_focus();
        }
    }
}

void root::tick(float dt_ms) {
    if (m_popup_pending_) m_popup_pending_.reset();
    if (m_context_menu_pending_) m_context_menu_pending_.reset();
    if (application::instance()->dispatcher()) {
        application::instance()->dispatcher()->drain_ui_queue();
    }
    container::tick(dt_ms);
    if (m_popup) m_popup->tick(dt_ms);
    if (m_context_menu) m_context_menu->tick(dt_ms);
}

void root::layout() {
    on_layout_begin();

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
                          std::function<void()> callback,
                          const std::string& owner) {
#ifdef __APPLE__
    std::string name_local = menu_name;
    std::string label_local = label;
    if (label_local.empty()) return false;
    std::string title = i18n_manager::get().tr(name_local, name_local);
    if (title.empty()) title = name_local;

    application::instance()->logger()->log({logging::level::trace,
        fmt::format("[macos_menu] root add_menu_item menu='{}' label='{}' title='{}' "
                    "(ptr label={} title={})",
                    name_local, label_local, title,
                    static_cast<const void*>(label_local.c_str()),
                    static_cast<const void*>(title.c_str())), "ui/root"});

    spiration_macos_menu_spec spec{};
    spec.key = name_local.c_str();
    spec.title = title.c_str();
    void* submenu = spiration_macos_menu_get_submenu(&spec);
    if (!submenu) return false;

    // 标题与回调放于同一堆对象：标题经堆内字符串读取，不依赖任何栈帧。
    auto* state = new spiration_menu_item_state{label_local, std::move(callback)};
    application::instance()->logger()->log({logging::level::trace,
        fmt::format("[macos_menu] root add_menu_item state={} title_ptr={} title='{}'",
                    static_cast<const void*>(state),
                    static_cast<const void*>(state->title.c_str()),
                    state->title), "ui/root"});

    spiration_macos_menu_spec item_spec{};
    item_spec.submenu = submenu;
    item_spec.callback = state;
    return spiration_macos_menu_add_item(&item_spec) != 0;
#else
    if (!menu_bar_) return false;
    return menu_bar_->add_sub_item(menu_name, label, std::move(callback), owner);
#endif
}

size_t root::remove_menu_items(const std::string& owner) {
    if (!menu_bar_) return 0;
    return menu_bar_->remove_owned_items(owner);
}

void root::open_tab(std::unique_ptr<tab> t) {
    if (!tab_bar_ || !t) return;
    tab_bar_->add_tab(std::move(t));
}

void root::show_popup(float x, float y, std::unique_ptr<popup_menu> popup) {
    dismiss_popup();
    m_popup_pending_.reset();
    popup->set_dismiss_callback([this]() { dismiss_popup(); });
    popup->init();
    if (x + popup->width > width) x = std::max(0.0f, width - popup->width);
    if (y + popup->height > height) y = std::max(0.0f, height - popup->height);
    popup->x = x;
    popup->y = y;
    if (request_repaint_) popup->set_repaint_callback(request_repaint_);
    if (window_action_) popup->set_window_action_callback(window_action_);
    m_popup = std::move(popup);
    if (request_repaint_) request_repaint_();
}

void root::dismiss_popup() {
    if (m_popup) {
        m_popup_pending_ = std::move(m_popup);
        if (request_repaint_) request_repaint_();
    }
}

void root::show_context_menu(float x, float y, std::unique_ptr<context_menu> menu) {
    if (m_context_menu) m_context_menu.reset();
    m_context_menu_pending_.reset();
    if (!menu) return;
    if (x + menu->width > width) x = std::max(0.0f, width - menu->width);
    if (y + menu->height > height) y = std::max(0.0f, height - menu->height);
    menu->set_repaint_callback(request_repaint_);
    menu->set_window_action_callback(window_action_);
    menu->on_dismiss = [this]() { dismiss_context_menu(); };
    menu->show_at(x, y);
    m_context_menu = std::move(menu);
    if (request_repaint_) request_repaint_();
}

void root::dismiss_context_menu() {
    if (m_context_menu) {
        m_context_menu->on_dismiss = nullptr;
        m_context_menu_pending_ = std::move(m_context_menu);
        if (request_repaint_) request_repaint_();
    }
}

} // namespace spiration
