/**
 * @file agent_tab.cpp
 * @brief 智能体标签页实现。
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

    auto rgroup = std::make_unique<container>();
    rgroup->set_layout_manager(std::make_unique<horizontal_layout>(2.0f));
    rgroup->widget_style.width = 124;
    rgroup->widget_style.margin = margin(6, 0);
    rgroup->widget_style.background_color = color::transparent();
    auto mk_rbtn = [this, &rgroup](const char* key, reasoning_level lvl) {
        auto b = std::make_unique<button>();
        b->text = i18n_manager::get().tr(key);
        b->widget_style.width = 40;
        b->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
        b->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
        b->on_click = [this, lvl]() { set_reasoning_level(lvl); };
        button* raw = b.get();
        rgroup->add_child(std::move(b));
        reasoning_btns_.push_back(raw);
    };
    mk_rbtn("reasoning.none", reasoning_level::none);
    mk_rbtn("reasoning.standard", reasoning_level::standard);
    mk_rbtn("reasoning.deep", reasoning_level::deep);
    bar->add_child(std::move(rgroup));

    auto tf = std::make_unique<text_field>();
    tf->placeholder = i18n_manager::get().tr("tab.agent.input_placeholder");
    tf->font_size = 13.0f;
    tf->widget_style.margin = margin(6, 0);
    tf->on_submit = [this](const std::string&) { send(); };
    input_ = tf.get();
    bar->add_child(std::move(tf));

    auto btn = std::make_unique<button>();
    btn->text = i18n_manager::get().tr("tab.agent.send");
    btn->widget_style.width = 56;
    btn->widget_style.margin = margin(6, 0);
    btn->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    btn->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    btn->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    btn->on_click = [this]() {
        if (waiting_) {
            stop_requested_ = true;
        } else {
            send();
        }
    };
    send_btn_ = btn.get();
    bar->add_child(std::move(btn));

    auto rp = std::make_unique<container>();
    rp->widget_style.width = 8; rp->widget_style.background_color = color::transparent();
    bar->add_child(std::move(rp));

    add_child(std::move(bar));

    update_reasoning_buttons();
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
    bool at_bottom = true;
    if (scroll_ && scroll_->scroll_max_y() > 0.0f) {
        at_bottom = scroll_->scroll_offset_y() >= scroll_->scroll_max_y() - 1.0f;
    }

    widget* w = this;
    while (w) {
        w->layout();
        w = w->parent();
    }

    if (scroll_ && at_bottom) {
        scroll_->scroll_to_y(99999.0f);
    }
    if (request_repaint_) request_repaint_();
}

void agent_tab::process_stream_events(size_t max_events) {
    std::deque<stream_event> events;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        if (max_events > 0 && stream_events_.size() > max_events) {
            for (size_t i = 0; i < max_events; ++i) {
                events.push_back(std::move(stream_events_.front()));
                stream_events_.pop_front();
            }
        } else {
            events.swap(stream_events_);
        }
    }
    if (events.empty()) return;

    bool need_layout = false;
    bool need_repaint = false;

    for (size_t k = 0; k < events.size();) {
        stream_event& ev = events[k];
        if (ev.t == stream_event::type::delta ||
            ev.t == stream_event::type::reasoning) {
            std::string batch = ev.text;
            size_t k2 = k + 1;
            while (k2 < events.size() && events[k2].t == ev.t) {
                batch += events[k2].text;
                ++k2;
            }
            if (ev.t == stream_event::type::reasoning) {
                if (batch.find_first_not_of(" \t\r\n") == std::string::npos) {
                    k = k2;
                    continue;
                }
                if (!thinking_capsule_) {
                    thinking_capsule_ = create_capsule(i18n_manager::get().tr("chat.thinking"));
                    thinking_scroll_ = static_cast<container*>(thinking_capsule_->content());
                    auto md = std::make_unique<markdown>();
                    md->font_size = 12.0f;
                    md->selectable = true;
                    thinking_label_ = md.get();
                    thinking_scroll_->add_child(std::move(md));
                    need_layout = true;
                }
                if (thinking_label_) {
                    float old_h = thinking_label_->height;
                    thinking_label_->text += batch;
                    thinking_label_->layout();
                    if (thinking_label_->height - old_h > 0.5f) need_layout = true;
                }
            } else {
                if (!stream_label_) {
                    add_message("assistant", "");
                    stream_label_ = last_label_;
                    stream_msg_index_ = messages_.size() - 1;
                    need_layout = true;
                }
                float old_h = stream_label_->height;
                stream_label_->text += batch;
                if (stream_msg_index_ < messages_.size())
                    messages_[stream_msg_index_].content = stream_label_->text;
                stream_label_->layout();
                if (stream_label_->height - old_h > 0.5f) need_layout = true;
            }
            need_repaint = true;
            k = k2;
            continue;
        }
        switch (ev.t) {
            case stream_event::type::tool_call:
                stream_label_ = nullptr;
                thinking_capsule_ = nullptr;
                thinking_scroll_ = nullptr;
                thinking_label_ = nullptr;
                append_tool_capsule(i18n_manager::get().tr("chat.tool_invoke"));
                need_layout = true;
                break;
            case stream_event::type::tool_result:
                stream_label_ = nullptr;
                append_tool_result(ev.text.empty() ? i18n_manager::get().tr("chat.no_output")
                                                   : ev.text);
                need_layout = true;
                break;
            default:
                break;
        }
        ++k;
    }

    if (need_layout) {
        relayout_scroll_repaint();
    } else if (need_repaint && request_repaint_) {
        request_repaint_();
    }
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
    std::string text = input_->text;
    input_->text.clear();

    if (waiting_) {
        add_message("user", text);
        queued_inputs_.push_back(text);
        supplement_requested_ = true;
        if (request_repaint_) request_repaint_();
        return;
    }
    start_send(text, true);
}

void agent_tab::start_send(const std::string& text, bool show_user) {
    if (show_user) {
        add_message("user", text);
    }
    if (!client_) {
        add_message("assistant", i18n_manager::get().tr("chat.echo") + " " + text);
        update_send_button();
        if (request_repaint_) request_repaint_();
        return;
    }

    client_->set_reasoning_level(reasoning_);
    thinking_capsule_ = nullptr;
    thinking_scroll_ = nullptr;
    thinking_label_ = nullptr;
    client_->add_message({"user", text, "", "", {}});
    waiting_ = true;
    stop_requested_ = false;
    supplement_requested_ = false;
    remove_continue_button();
    update_send_button();

    auto alive = alive_;
    chat_events events;
    events.on_delta = [this, alive](const std::string& delta) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_event ev;
        ev.t = stream_event::type::delta;
        ev.text = delta;
        stream_events_.push_back(std::move(ev));
    };
    events.on_reasoning_delta = [this, alive](const std::string& delta) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_event ev;
        ev.t = stream_event::type::reasoning;
        ev.text = delta;
        stream_events_.push_back(std::move(ev));
    };
    events.on_tool_call = [this, alive](const tool_call& tc) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_event ev;
        ev.t = stream_event::type::tool_call;
        ev.text = tc.function_name;
        stream_events_.push_back(std::move(ev));
    };
    events.on_tool_result = [this, alive](const tool_execution& ex) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_event ev;
        ev.t = stream_event::type::tool_result;
        ev.text = ex.result;
        if (ev.text.size() > 2000)
            ev.text = ev.text.substr(0, 2000) + "\n" + i18n_manager::get().tr("chat.result_truncated");
        stream_events_.push_back(std::move(ev));
    };
    events.should_stop = [this, alive]() {
        return !alive->load() || stop_requested_.load();
    };
    events.should_yield = [this, alive]() {
        return !alive->load() || supplement_requested_.load();
    };

    pending_ = std::async(std::launch::async, [this, events, alive]() {
        if (!alive->load()) return chat_response{};
        return client_->run(100, events);
    });
    if (request_repaint_) request_repaint_();
}

void agent_tab::continue_generation() {
    if (waiting_ || !client_) return;
    start_send("Continue.", false);
}

void agent_tab::add_continue_button() {
    if (continue_btn_) return;

    auto row = std::make_unique<container>();
    row->set_layout_manager(std::make_unique<horizontal_layout>(0.0f));
    row->widget_style.height = 34.0f;
    row->widget_style.background_color = color::transparent();

    auto btn = std::make_unique<button>();
    btn->text = i18n_manager::get().tr("chat.continue");
    btn->widget_style.width = 110;
    btn->widget_style.margin = margin(0, 0);
    btn->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    btn->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    btn->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    btn->on_click = [this]() { continue_generation(); };
    continue_btn_ = btn.get();
    row->add_child(std::move(btn));
    msg_list_->add_child(std::move(row));
    relayout_scroll_repaint();
}

void agent_tab::remove_continue_button() {
    if (continue_btn_ && msg_list_) {
        if (auto* p = continue_btn_->parent()) {
            msg_list_->remove_child(p);
        }
    }
    continue_btn_ = nullptr;
}

void agent_tab::update_send_button() {
    if (!send_btn_) return;
    send_btn_->text = waiting_
        ? i18n_manager::get().tr("chat.stop")
        : i18n_manager::get().tr("tab.agent.send");
}

void agent_tab::set_reasoning_level(reasoning_level l) {
    reasoning_ = l;
    if (client_) client_->set_reasoning_level(l);
    update_reasoning_buttons();
    if (request_repaint_) request_repaint_();
}

void agent_tab::update_reasoning_buttons() {
    const reasoning_level levels[] = {reasoning_level::none,
                                      reasoning_level::standard,
                                      reasoning_level::deep};
    for (size_t i = 0; i < reasoning_btns_.size() && i < 3; ++i) {
        bool active = (reasoning_ == levels[i]);
        reasoning_btns_[i]->set_base_bg(active
            ? theme_manager::get(theme_manager::BUTTON_HOVER)
            : theme_manager::get(theme_manager::CONTENT_BG));
    }
}

collapsible* agent_tab::create_capsule(const std::string& summary) {
    auto cap = std::make_unique<collapsible>();
    cap->set_summary(summary);
    cap->max_content_height = 200.0f;
    cap->set_expanded(false);

    auto sc = std::make_unique<container>();
    sc->widget_style.overflow_y = true;
    sc->widget_style.background_color = color::transparent();
    sc->widget_style.padding = margin(4, 6);
    cap->set_content(std::move(sc));

    collapsible* raw = cap.get();
    msg_list_->add_child(std::move(cap));
    relayout_scroll_repaint();
    return raw;
}

collapsible* agent_tab::append_tool_capsule(const std::string& summary) {
    tool_capsule_ = create_capsule(summary);
    tool_capsule_scroll_ = static_cast<container*>(tool_capsule_->content());
    return tool_capsule_;
}

void agent_tab::append_tool_result(const std::string& text) {
    if (tool_capsule_ && tool_capsule_scroll_) {
        auto lb = std::make_unique<label>();
        lb->text = text;
        lb->font_size = 12.0f;
        lb->selectable = true;
        tool_capsule_scroll_->add_child(std::move(lb));
    }
    tool_capsule_ = nullptr;
    tool_capsule_scroll_ = nullptr;
    relayout_scroll_repaint();
}

void agent_tab::tick(float dt_ms) {
    if (waiting_)
        process_stream_events(300);

    if (waiting_ && pending_.valid()) {
        auto status = pending_.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            chat_response resp;
            try {
                resp = pending_.get();
            } catch (const std::exception& e) {
                waiting_ = false;
                update_send_button();
                std::string err = e.what();
                if (err.size() > 200) err = err.substr(0, 200);
                add_message("assistant", i18n_manager::get().tr("chat.request_failed") + " " + err);
                if (!queued_inputs_.empty()) {
                    std::string next = queued_inputs_.front();
                    queued_inputs_.pop_front();
                    start_send(next, false);
                }
                if (request_repaint_) request_repaint_();
                return;
            }
            waiting_ = false;
            update_send_button();

            process_stream_events();

            if (!resp.content.empty() && stream_label_) {
                stream_label_->text = resp.content;
                if (stream_msg_index_ < messages_.size())
                    messages_[stream_msg_index_].content = resp.content;
                relayout_scroll_repaint();
            } else if (!resp.content.empty() && !stream_label_) {
                add_message("assistant", resp.content);
            }

            stream_label_ = nullptr;

            if (stop_requested_.load() && queued_inputs_.empty()) {
                add_continue_button();
            }

            if (!queued_inputs_.empty()) {
                std::string next = queued_inputs_.front();
                queued_inputs_.pop_front();
                start_send(next, false);
            }
            if (request_repaint_) request_repaint_();
        }
    }
    widget::tick(dt_ms);
}

} // namespace agent
} // namespace spiration
