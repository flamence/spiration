/**
 * @file agent_tab.cpp
 * @brief 智能体标签页实现�?
 * @author clk
 */

#include <extension/builtin/agent/agent_tab.h>
#include <extension/builtin/i18n/i18n.h>

#include <exception>

namespace spiration {
namespace agent {

agent_tab::agent_tab(chat_client* client) : client_(client) {
    title_ = i18n_manager::get().tr("tab.agent");
    widget_style.background_color = theme_manager::get(theme_manager::CONTENT_BG);

    auto sv = std::make_unique<container>();
    sv->widget_style.overflow_y = true;
    sv->widget_style.background_color = color::transparent();
    scroll_ = sv.get();

    auto ml = std::make_unique<container>();
    ml->set_layout_manager(std::make_unique<vertical_layout>(8.0f));
    ml->widget_style.padding = margin(10, 12);
    ml->widget_style.background_color = color::transparent();
    msg_list_ = ml.get();
    scroll_->add_child(std::move(ml));

    add_child(std::move(sv));

    auto bar = std::make_unique<container>();
    bar->set_layout_manager(std::make_unique<horizontal_layout>(8.0f));
    bar->height = 46.0f;
    bar->widget_style.background_color = color::transparent();

    auto lp = std::make_unique<container>();
    lp->widget_style.width = 8; lp->widget_style.background_color = color::transparent();
    bar->add_child(std::move(lp));

    auto tf = std::make_unique<text_field>();
    tf->placeholder = i18n_manager::get().tr("tab.agent.input_placeholder");
    tf->font_size = 13.0f;
    tf->on_submit = [this](const std::string&) { send(); };
    input_ = tf.get();
    bar->add_child(std::move(tf));

    auto btn = std::make_unique<button>();
    btn->text = i18n_manager::get().tr("tab.agent.send");
    btn->width = 56.0f;
    btn->widget_style.width = 56;
    btn->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    btn->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    btn->widget_style.background_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    btn->on_click = [this]() { send(); };
    send_btn_ = btn.get();
    bar->add_child(std::move(btn));

    auto rp = std::make_unique<container>();
    rp->widget_style.width = 8; rp->widget_style.background_color = color::transparent();
    bar->add_child(std::move(rp));

    add_child(std::move(bar));
}

void agent_tab::paint(std::shared_ptr<renderer> renderer) {
    if (widget_style.background_color.a > 0.0f) {
        renderer->draw_rectangle({0, 0, width, height}, widget_style.background_color);
    }
    widget::paint(renderer);
}

void agent_tab::handle_event(const event_type& type, void* data) {
    if (type == event_type::keyboard) {
        if (input_) input_->handle_event(type, data);
        if (!static_cast<key_event_data*>(data)->consumed && msg_list_)
            msg_list_->handle_event(type, data);
        return;
    }
    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        if (md->action == mouse_action::down && input_ && !input_->focused()) {
            input_->focus();
        }
    }
    container::handle_event(type, data);
}

void agent_tab::layout() {
    float bar_h = 46.0f;
    float pad = 12.0f;
    for (auto& child : children()) {
        if (child.get() == scroll_) {
            child->x = 0;
            child->y = 0;
            child->width = width;
            child->height = height - bar_h - pad;
            child->layout();
        } else {
            child->x = pad;
            child->y = height - bar_h - pad;
            child->width = width - pad * 2;
            child->height = bar_h;
            child->layout();
        }
    }
}

void agent_tab::add_message(const std::string& role, const std::string& content) {
    messages_.push_back({role, content});
    last_label_ = append_bubble(messages_.back());
}

void agent_tab::relayout_scroll_repaint() {
    widget* w = this;
    while (w) {
        w->layout();
        w = w->parent();
    }
    if (scroll_) scroll_->scroll_to_y(99999.0f);
    if (request_repaint_) request_repaint_();
}

void agent_tab::process_stream_events() {
    std::deque<stream_event> events;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        events.swap(stream_events_);
    }
    if (events.empty()) return;

    for (auto& ev : events) {
        switch (ev.t) {
            case stream_event::type::delta: {
                if (!stream_label_) {
                    add_message("assistant", "");
                    stream_label_ = last_label_;
                }
                stream_label_->text += ev.text;
                if (!messages_.empty())
                    messages_.back().content = stream_label_->text;
                break;
            }
            case stream_event::type::tool_call:
                stream_label_ = nullptr;
                add_message("assistant", i18n_manager::get().tr("chat.tool_call", {ev.text}));
                break;
            case stream_event::type::tool_result:
                stream_label_ = nullptr;
                add_message("tool", ev.text.empty() ? i18n_manager::get().tr("chat.no_output") : ev.text);
                break;
        }
    }
    relayout_scroll_repaint();
}

void agent_tab::on_activate() {
    if (scroll_ && scroll_->width > 0) {
        scroll_->layout();
        scroll_->scroll_to_y(99999.0f);
    }
    if (request_repaint_) request_repaint_();
}

markdown* agent_tab::append_bubble(const display_message& msg) {
    bool is_user = (msg.role == "user");

    auto md = std::make_unique<markdown>();
    md->text = msg.content;
    md->font_size = 13.0f;
    md->selectable = true;
    md->h_align = is_user ? text_alignment::right : text_alignment::left;
    md->v_align = vertical_alignment::top;

    markdown* raw = md.get();
    msg_list_->add_child(std::move(md));

    relayout_scroll_repaint();
    return raw;
}

void agent_tab::send() {
    if (!input_ || input_->text.empty()) return;
    if (waiting_) return;

    std::string text = input_->text;
    input_->text.clear();

    add_message("user", text);

    if (client_) {
        client_->add_message({"user", text, "", "", {}});
        waiting_ = true;
        if (send_btn_) send_btn_->enabled = false;

        chat_events events;
        events.on_delta = [this](const std::string& delta) {
            std::lock_guard<std::mutex> lock(stream_mutex_);
            stream_event ev;
            ev.t = stream_event::type::delta;
            ev.text = delta;
            stream_events_.push_back(std::move(ev));
        };
        events.on_tool_call = [this](const tool_call& tc) {
            std::lock_guard<std::mutex> lock(stream_mutex_);
            stream_event ev;
            ev.t = stream_event::type::tool_call;
            ev.text = tc.function_name;
            stream_events_.push_back(std::move(ev));
        };
        events.on_tool_result = [this](const tool_execution& ex) {
            std::lock_guard<std::mutex> lock(stream_mutex_);
            stream_event ev;
            ev.t = stream_event::type::tool_result;
            ev.text = ex.result;
            if (ev.text.size() > 300)
                ev.text = ev.text.substr(0, 300) + "\n" + i18n_manager::get().tr("chat.result_truncated");
            stream_events_.push_back(std::move(ev));
        };

        pending_ = std::async(std::launch::async, [this, events]() {
            return client_->run(10, events);
        });
    } else {
        add_message("assistant", i18n_manager::get().tr("chat.echo") + " " + text);
    }

    if (request_repaint_) request_repaint_();
}

void agent_tab::tick(float dt_ms) {
    if (waiting_)
        process_stream_events();

    if (waiting_ && pending_.valid()) {
        auto status = pending_.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            chat_response resp;
            try {
                resp = pending_.get();
            } catch (const std::exception& e) {
                waiting_ = false;
                if (send_btn_) send_btn_->enabled = true;
                std::string err = e.what();
                if (err.size() > 200) err = err.substr(0, 200);
                add_message("assistant", i18n_manager::get().tr("chat.request_failed") + " " + err);
                if (request_repaint_) request_repaint_();
                return;
            }
            waiting_ = false;
            if (send_btn_) send_btn_->enabled = true;

            process_stream_events();

            if (!resp.content.empty() && stream_label_) {
                stream_label_->text = resp.content;
                if (!messages_.empty())
                    messages_.back().content = resp.content;
                relayout_scroll_repaint();
            } else if (!resp.content.empty() && !stream_label_) {
                add_message("assistant", resp.content);
            }

            if (resp.content.empty() && resp.executions.empty()) {
                add_message("assistant", i18n_manager::get().tr("chat.no_reply"));
            }
            stream_label_ = nullptr;
            if (request_repaint_) request_repaint_();
        }
    }
    widget::tick(dt_ms);
}

} // namespace agent
} // namespace spiration
