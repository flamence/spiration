/**
 * @file agent_tab.cpp
 * @brief 智能体标签页实现。
 * @author clk
 */

#include <extension/builtin/agent/agent_tab.h>
#include <extension/builtin/i18n/i18n.h>
#include <ui/context_menu.h>

#include <algorithm>
#include <exception>

namespace spiration {
namespace agent {

namespace {

constexpr float BACKBAR_H  = 34.0f;   ///< 竖屏返回栏高度
constexpr float SETTINGS_H = 34.0f;   ///< 底部设置栏高度
constexpr float INPUTBAR_H = 46.0f;   ///< 输入栏高度
constexpr float PAD        = 12.0f;   ///< 输入栏外边距

constexpr int VK_ESCAPE = 0x1B;

std::string tool_display_name(const std::string& name) {
    return i18n_manager::get().tr("tool." + name, name);
}

std::string tool_block_markdown(const std::string& label, const std::string& body) {
    return "**" + label + "**\n```\n" + body + "\n```\n";
}

float token_text_estimate_width(const label* lb) {
    float tw = lb->text_width();
    if (tw > 0.0f) return tw;
    size_t chars = 0;
    const std::string& s = lb->text;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) ++chars;
    }
    float fs = lb->font_size > 0.0f ? lb->font_size : 12.0f;
    return static_cast<float>(chars) * fs * 0.6f;
}

} // namespace

agent_tab::agent_tab(chat_client* client, chat_store* store) : client_(client), store_(store) {
    title_ = i18n_manager::get().tr("tab.agent");
    widget_style.background_color = theme_manager::get(theme_manager::CONTENT_BG);

    if (client_) {
        client_->on_tokens_updated = [this, alive = alive_]() {
            if (alive->load()) tokens_dirty_.store(true);
        };
    }

    auto sp = std::make_unique<split_pane>();
    sp->dir = split_pane::direction::horizontal;
    sp->widget_style.background_color = color::transparent();
    sp->min_ratio = 0.02f;
    sp->max_ratio = 0.9f;
    sp->min_first_px = 120.0f;
    sp->min_second_px = 260.0f;
    sp->set_split_ratio(0.3f);
    split_pane_ = sp.get();
    sp->on_ratio_changed = [this]() { relayout_scroll_repaint(); };

    auto lp = std::make_unique<container>();
    lp->widget_style.background_color = color::transparent();
    list_pane_ = lp.get();

    auto lh = std::make_unique<container>();
    lh->widget_style.background_color = color::transparent();
    list_header_ = lh.get();
    list_pane_->add_child(std::move(lh));

    auto toggle = std::make_unique<button>();
    toggle->text = "\xE2\x98\xB0";  // ☰
    toggle->set_base_bg(color::transparent());
    toggle->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    toggle->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    toggle->on_click = [this]() {
        if (!split_pane_) return;
        if (!list_collapsed_) {
            list_collapsed_ = true;
            float avail = std::max(1.0f, split_pane_->width - split_pane_->handle_size);
            split_pane_->set_split_ratio(36.0f / avail);
        } else {
            list_collapsed_ = false;
            split_pane_->set_split_ratio(std::min(0.5f, list_expand_ratio_));
        }
        relayout_scroll_repaint();
    };
    toggle_list_ = toggle.get();
    list_header_->add_child(std::move(toggle));

    auto nb = std::make_unique<button>();
    nb->text = "\xEF\xBC\x8B";
    nb->set_base_bg(color::transparent());
    nb->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    nb->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    nb->on_click = [this]() { new_conversation(); };
    new_btn_ = nb.get();
    list_header_->add_child(std::move(nb));

    auto lsc = std::make_unique<container>();
    lsc->widget_style.overflow_y = true;
    lsc->widget_style.background_color = color::transparent();
    lsc->widget_style.padding = margin(4, 4);
    list_scroll_ = lsc.get();
    list_pane_->add_child(std::move(lsc));

    split_pane_->add_child(std::move(lp));

    auto cp = std::make_unique<container>();
    cp->widget_style.background_color = color::transparent();
    chat_pane_ = cp.get();

    auto bb = std::make_unique<container>();
    bb->widget_style.background_color = theme_manager::get(theme_manager::CONTENT_BG);
    back_bar_ = bb.get();
    chat_pane_->add_child(std::move(bb));

    auto back = std::make_unique<button>();
    back->text = "\xE2\x86\x90";
    back->h_align = text_alignment::left;
    back->widget_style.padding = margin(0, 0, 0, 6);
    back->set_base_bg(color::transparent());
    back->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    back->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    back->on_click = [this]() {
        show_chat_ = false;
        relayout_scroll_repaint();
    };
    back_btn_ = back.get();
    back_bar_->add_child(std::move(back));

    auto sv = std::make_unique<container>();
    sv->widget_style.overflow_y = true;
    sv->widget_style.background_color = color::transparent();
    scroll_ = sv.get();
    chat_pane_->add_child(std::move(sv));

    auto ml = std::make_unique<container>();
    ml->set_layout_manager(std::make_unique<vertical_layout>(8.0f));
    ml->widget_style.padding = margin(10, 12);
    ml->widget_style.background_color = color::transparent();
    msg_list_ = ml.get();
    scroll_->add_child(std::move(ml));

    auto tc = std::make_unique<collapsible>();
    tc->set_summary(i18n_manager::get().tr("chat.todo"));
    tc->max_content_height = 200.0f;
    tc->set_expanded(false);
    todo_capsule_ = tc.get();

    auto tv_scroll = std::make_unique<container>();
    tv_scroll->widget_style.overflow_y = true;
    tv_scroll->widget_style.background_color = color::transparent();
    auto tv = std::make_unique<todo_view>();
    tv->on_content_changed = [this]() { update_todo_capsule(); };
    todo_view_ = tv.get();
    tv_scroll->add_child(std::move(tv));
    tc->set_content(std::move(tv_scroll));

    chat_pane_->add_child(std::move(tc));

    auto sb = std::make_unique<container>();
    sb->widget_style.background_color = theme_manager::get(theme_manager::CONTENT_BG);
    settings_bar_ = sb.get();
    chat_pane_->add_child(std::move(sb));

    auto mc = std::make_unique<combo_box>();
    mc->font_size = 12.0f;
    mc->item_height = 22.0f;
    mc->on_changed = [this](int idx) {
        if (!client_ || !model_combo_) return;
        if (idx >= 0 && idx < static_cast<int>(models_.size())) {
            client_->configure(models_[static_cast<size_t>(idx)].cfg);
            save_current();
        }
    };
    mc->popup_up = true;
    model_combo_ = mc.get();
    settings_bar_->add_child(std::move(mc));

    auto rc = std::make_unique<combo_box>();
    rc->font_size = 12.0f;
    rc->item_height = 22.0f;
    rc->items = {i18n_manager::get().tr("reasoning.none"),
                 i18n_manager::get().tr("reasoning.standard"),
                 i18n_manager::get().tr("reasoning.deep")};
    rc->selected_index = 1;
    rc->on_changed = [this](int idx) {
        reasoning_level lvl = reasoning_level::standard;
        if (idx == 0)      lvl = reasoning_level::none;
        else if (idx == 2) lvl = reasoning_level::deep;
        set_reasoning_level(lvl);
    };
    reasoning_combo_ = rc.get();
    rc->popup_up = true;
    settings_bar_->add_child(std::move(rc));

    auto ap = std::make_unique<checkbox>();
    ap->text = i18n_manager::get().tr("chat.auto_approve");
    ap->on_changed = [this](bool v) { auto_approve_on_ = v; };
    auto_approve_ = ap.get();
    settings_bar_->add_child(std::move(ap));

    auto tl = std::make_unique<label>();
    tl->text = "";
    tl->font_size = 12.0f;
    tl->h_align = text_alignment::right;
    tl->v_align = vertical_alignment::center;
    token_label_ = tl.get();
    settings_bar_->add_child(std::move(tl));

    auto bar = std::make_unique<container>();
    bar->set_layout_manager(std::make_unique<horizontal_layout>(8.0f));
    bar->widget_style.background_color = color::transparent();
    input_bar_ = bar.get();

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

    chat_pane_->add_child(std::move(bar));
    split_pane_->add_child(std::move(cp));
    add_child(std::move(sp));

    auto rd = std::make_unique<input_dialog>();
    rename_dialog_ = rd.get();
    add_child(std::move(rd));

    if (client_ && !client_->get_config().model.empty()) {
        model_combo_->items = {client_->get_config().model};
        model_combo_->selected_index = 0;
    }

    if (store_) {
        initial_load_ = std::async(std::launch::async, [store = store_]() {
            initial_load_result res;
            res.convos = store->list();
            if (!res.convos.empty()) {
                res.uuid = res.convos.front().uuid;
                res.loaded = store->load(res.uuid, res.archive);
            }
            return res;
        });
    }
}

void agent_tab::set_models(std::vector<model_option> models) {
    if (!model_combo_) return;
    models_ = std::move(models);
    if (models_.empty() && client_) {
        model_option opt;
        opt.display_name = client_->get_config().model;
        opt.cfg = client_->get_config();
        models_.push_back(std::move(opt));
    }
    model_combo_->items.clear();
    for (const auto& m : models_) model_combo_->items.push_back(m.display_name);
    if (client_) {
        std::string cur = client_->get_config().model;
        for (size_t i = 0; i < models_.size(); ++i) {
            if (models_[i].cfg.model == cur || models_[i].display_name == cur) {
                model_combo_->selected_index = static_cast<int>(i);
                break;
            }
        }
    }
    if (model_combo_->items.empty()) model_combo_->selected_index = -1;
    if (request_repaint_) request_repaint_();
}

void agent_tab::paint(std::shared_ptr<renderer> renderer) {
    if (widget_style.background_color.a > 0.0f) {
        renderer->draw_rectangle({0, 0, width, height}, widget_style.background_color);
    }
    widget::paint(renderer);
}

void agent_tab::handle_event(const event_type& type, void* data) {
    if (rename_dialog_ && rename_dialog_->visible()) {
        rename_dialog_->handle_event(type, data);
        return;
    }

    bool list_visible = landscape_ || !show_chat_;
    bool chat_visible = landscape_ || show_chat_;

    if (type == event_type::mouse) {
        auto* md = static_cast<mouse_event_data*>(data);
        point orig = md->position;

        if (landscape_ && split_pane_) {
            md->position.x = orig.x - split_pane_->x;
            md->position.y = orig.y - split_pane_->y;
            split_pane_->handle_event(type, data);
        } else {
            if (list_visible && list_pane_) {
                md->position.x = orig.x - list_pane_->x;
                md->position.y = orig.y - list_pane_->y;
                list_pane_->handle_event(type, data);
            }
            if (!md->consumed && chat_visible && chat_pane_) {
                md->position = orig;
                md->position.x = orig.x - chat_pane_->x;
                md->position.y = orig.y - chat_pane_->y;
                chat_pane_->handle_event(type, data);
            }
        }
        md->position = orig;
        return;
    }
    if (type == event_type::keyboard) {
        if (chat_visible && input_) input_->handle_event(type, data);
        if (!static_cast<key_event_data*>(data)->consumed && chat_visible && msg_list_)
            msg_list_->handle_event(type, data);
        return;
    }
    container::handle_event(type, data);
}

void agent_tab::layout() {
    landscape_ = width >= height;

    if (landscape_) {
        split_pane_->x = 0;
        split_pane_->y = 0;
        split_pane_->width = width;
        split_pane_->height = height;
        split_pane_->dir = split_pane::direction::horizontal;
        split_pane_->show_handle = true;
        split_pane_->auto_layout = true;
        split_pane_->layout();

        list_collapsed_ = list_pane_->width < 40.0f;
        if (!list_collapsed_) list_expand_ratio_ = split_pane_->split_ratio();

        layout_list_internal();
        layout_chat_internal();
    } else {
        split_pane_->show_handle = false;
        split_pane_->auto_layout = false;
        split_pane_->x = 0;
        split_pane_->y = 0;
        split_pane_->width = width;
        split_pane_->height = height;

        if (show_chat_) {
            list_pane_->x = 0; list_pane_->y = 0; list_pane_->width = 0; list_pane_->height = 0;
            chat_pane_->x = 0; chat_pane_->y = 0; chat_pane_->width = width; chat_pane_->height = height;
            layout_chat_internal();
        } else {
            chat_pane_->x = 0; chat_pane_->y = 0; chat_pane_->width = 0; chat_pane_->height = 0;
            list_pane_->x = 0; list_pane_->y = 0; list_pane_->width = width; list_pane_->height = height;
            layout_list_internal();
        }
    }

    if (rename_dialog_) {
        rename_dialog_->x = 0;
        rename_dialog_->y = 0;
        rename_dialog_->width = width;
        rename_dialog_->height = height;
        rename_dialog_->layout();
    }

    widget::layout();
}

void agent_tab::layout_list_internal() {
    float lw = list_pane_->width;
    bool collapsed = list_collapsed_ || lw < 40.0f;

    list_header_->x = 0;
    list_header_->y = 0;
    list_header_->width = lw;
    list_header_->height = 40.0f;

    toggle_list_->x = 4;
    toggle_list_->y = 5;
    if (landscape_) {
        toggle_list_->width = collapsed ? std::min(28.0f, std::max(0.0f, lw - 8.0f)) : 28.0f;
        toggle_list_->height = 28;
    } else {
        toggle_list_->width = 0;
        toggle_list_->height = 0;
    }
    toggle_list_->layout();

    float hx = landscape_ ? (4 + 28 + 4) : 4.0f;
    if (collapsed) {
        new_btn_->width = 0;
        new_btn_->height = 0;
    } else {
        new_btn_->x = hx;
        new_btn_->y = 5;
        new_btn_->width = 28;
        new_btn_->height = 28;
        new_btn_->layout();
    }

    list_scroll_->x = 0;
    list_scroll_->y = 40.0f;
    list_scroll_->width = lw;
    list_scroll_->height = collapsed ? 0.0f : std::max(0.0f, list_pane_->height - 40.0f);
    list_scroll_->layout();
}

void agent_tab::layout_chat_internal() {
    float cw = chat_pane_->width;
    float ch = chat_pane_->height;
    bool show_back = !landscape_ && show_chat_;

    if (show_back) {
        back_bar_->x = 0;
        back_bar_->y = 0;
        back_bar_->width = cw;
        back_bar_->height = BACKBAR_H;
        back_btn_->x = 8;
        back_btn_->y = 3;
        back_btn_->width = 40;
        back_btn_->height = BACKBAR_H - 6;
        back_btn_->layout();
    } else {
        back_bar_->width = 0;
        back_bar_->height = 0;
        back_btn_->width = 0;
        back_btn_->height = 0;
    }

    float settings_y = ch - SETTINGS_H - INPUTBAR_H - PAD;
    float capsule_h = 0.0f;
    if (todo_count_ > 0) {
        todo_capsule_->x = 0;
        todo_capsule_->width = cw;
        todo_capsule_->layout();
        capsule_h = todo_capsule_->height;
    } else {
        todo_capsule_->width = 0;
        todo_capsule_->height = 0;
    }
    float capsule_y = settings_y - capsule_h;
    if (todo_count_ > 0) todo_capsule_->y = capsule_y;

    settings_bar_->x = 0;
    settings_bar_->y = settings_y;
    settings_bar_->width = cw;
    settings_bar_->height = SETTINGS_H;

    const float GAP = 8.0f;
    const float TOKEN_MAX_W = 96.0f;
    const float AUTO_W = 100.0f;
    const float MODEL_W = 150.0f;
    const float REASON_W = 110.0f;
    const float MIN_COMBO_W = 30.0f;

    if (token_label_) {
        float tw = std::max(24.0f, std::min(TOKEN_MAX_W,
                                            token_text_estimate_width(token_label_) + 10.0f));
        token_label_->x = std::max(0.0f, cw - tw - GAP);
        token_label_->y = 2.0f;
        token_label_->width = tw;
        token_label_->height = SETTINGS_H - 4.0f;
        token_label_->layout();
        token_label_->y += std::max(0.0f, (SETTINGS_H - 4.0f - token_label_->height) * 0.5f);
    }

    float model_w = MODEL_W, reason_w = REASON_W;
    float token_left = token_label_ ? token_label_->x : cw;
    float left_end = GAP + AUTO_W + GAP + model_w + GAP + reason_w;
    float overlap = left_end - token_left;
    if (overlap > 0.0f) {
        float reduce = std::min(overlap, (REASON_W - MIN_COMBO_W) + (MODEL_W - MIN_COMBO_W));
        float dr = std::min(reduce, REASON_W - MIN_COMBO_W);
        reason_w -= dr; reduce -= dr;
        model_w -= std::min(reduce, MODEL_W - MIN_COMBO_W);
    }

    float sx = GAP;
    auto_approve_->x = sx;
    auto_approve_->y = 2.0f;
    auto_approve_->width = AUTO_W;
    auto_approve_->height = SETTINGS_H - 4.0f;
    auto_approve_->layout();
    sx += AUTO_W + GAP;

    model_combo_->x = sx;
    model_combo_->y = 2.0f;
    model_combo_->width = model_w;
    model_combo_->height = SETTINGS_H - 4.0f;
    model_combo_->layout();
    sx += model_w + GAP;

    reasoning_combo_->x = sx;
    reasoning_combo_->y = 2.0f;
    reasoning_combo_->width = reason_w;
    reasoning_combo_->height = SETTINGS_H - 4.0f;
    reasoning_combo_->layout();

    scroll_->x = 0;
    scroll_->y = show_back ? BACKBAR_H : 0.0f;
    scroll_->width = cw;
    scroll_->height = std::max(0.0f, capsule_y - scroll_->y);
    scroll_->layout();

    input_bar_->x = PAD;
    input_bar_->y = ch - INPUTBAR_H - PAD;
    input_bar_->width = std::max(0.0f, cw - PAD * 2);
    input_bar_->height = INPUTBAR_H;
    input_bar_->layout();
}

void agent_tab::add_message(const std::string& role, const std::string& content) {
    messages_.push_back({role, content});
    last_label_ = append_bubble(messages_.back());
}

void agent_tab::relayout_scroll_repaint() {
    if (layout_batch_) return;
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
                streamed_content_ = true;
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
                append_tool_capsule(tool_display_name(ev.name), ev.args);
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

widget* agent_tab::hit_test_hover(float x, float y) const {
    if (rename_dialog_ && rename_dialog_->visible())
        return rename_dialog_->hit_test_hover(x, y);
    return widget::hit_test_hover(x, y);
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

void agent_tab::save_current() {
    if (on_conversation_done) on_conversation_done(this);
}

void agent_tab::wait_initial_load() {
    if (initial_load_.valid()) initial_load_.wait();
}

void agent_tab::apply_initial_load(const initial_load_result& res) {
    if (!store_) return;
    if (res.convos.empty()) {
        new_conversation();
        return;
    }
    current_uuid_ = res.uuid.empty() ? res.convos.front().uuid : res.uuid;
    store_->set_current_uuid(current_uuid_);
    if (res.loaded) {
        if (client_) {
            client_->clear_history();
            for (const auto& m : res.archive.messages) {
                if (m.role == "system") continue;
                client_->add_message(m);
            }
            client_->set_tokens(res.archive.tokens_in, res.archive.tokens_out);
        }
        auto_approve_on_ = res.archive.auto_approve;
        if (auto_approve_) auto_approve_->checked = res.archive.auto_approve;
        todo_store::instance().set_current_uuid(current_uuid_);
        todo_store::instance().set(res.archive.todos);
        restore_history_ui(res.archive.messages);
        if (res.archive.can_continue) add_continue_button();
        update_token_label();
    }
    rebuild_conversation_list(&res.convos);
    relayout_scroll_repaint();
}

void agent_tab::clear_messages() {
    messages_.clear();
    if (msg_list_) {
        while (!msg_list_->children().empty())
            msg_list_->remove_child(msg_list_->children().front().get());
    }
    stream_label_ = nullptr;
    last_label_ = nullptr;
    thinking_capsule_ = nullptr;
    thinking_scroll_ = nullptr;
    thinking_label_ = nullptr;
    tool_capsule_ = nullptr;
    tool_capsule_scroll_ = nullptr;
    tool_label_ = nullptr;
    continue_btn_ = nullptr;
}

void agent_tab::update_todo_capsule() {
    if (!todo_capsule_) return;
    auto items = todo_store::instance().items();
    todo_count_ = items.size();
    size_t done = 0;
    for (const auto& it : items) {
        if (it.status == todo_status::completed) ++done;
    }
    std::string summary = i18n_manager::get().tr("chat.todo");
    if (todo_count_ > 0)
        summary += " (" + std::to_string(done) + "/" + std::to_string(todo_count_) + ")";
    todo_capsule_->set_summary(summary);

    bool now_has = (todo_count_ > 0);
    if (now_has && !todo_has_) todo_capsule_->set_expanded(true);  // 首现自动展开
    todo_has_ = now_has;

    relayout_scroll_repaint();
}

void agent_tab::new_conversation(bool to_chat) {
    if (!store_ || waiting_) return;
    save_current();
    std::string uuid = store_->create("");
    if (uuid.empty()) return;
    current_uuid_ = uuid;
    store_->set_current_uuid(uuid);
    if (client_) {
        client_->clear_history();
        client_->reset_tokens();
    }
    todo_store::instance().set_current_uuid(uuid);
    todo_store::instance().clear();
    clear_messages();
    update_token_label();
    if (to_chat) show_chat_ = true;
    request_list_rebuild();
    relayout_scroll_repaint();
}

void agent_tab::open_conversation(const std::string& uuid, bool to_chat) {
    if (!store_ || uuid.empty() || waiting_) return;
    if (uuid == current_uuid_) {
        if (to_chat) {
            show_chat_ = true;
            relayout_scroll_repaint();
        }
        return;
    }
    save_current();
    chat_archive archive;
    if (!store_->load(uuid, archive)) return;
    current_uuid_ = uuid;
    store_->set_current_uuid(uuid);
    if (client_) {
        client_->clear_history();
        for (const auto& m : archive.messages) {
            if (m.role == "system") continue;
            client_->add_message(m);
        }
        client_->set_tokens(archive.tokens_in, archive.tokens_out);
    }
    auto_approve_on_ = archive.auto_approve;
    if (auto_approve_) auto_approve_->checked = archive.auto_approve;
    todo_store::instance().set_current_uuid(uuid);
    todo_store::instance().set(archive.todos);
    clear_messages();
    restore_history_ui(archive.messages);
    if (archive.can_continue) add_continue_button();
    update_token_label();
    if (to_chat) show_chat_ = true;
    request_list_rebuild();
    relayout_scroll_repaint();
}

void agent_tab::delete_conversation(const std::string& uuid) {
    if (!store_ || uuid.empty() || waiting_) return;
    bool was_current = (uuid == current_uuid_);
    bool stay_view = show_chat_;
    store_->remove(uuid);
    todo_store::instance().remove(uuid);
    if (was_current) {
        current_uuid_.clear();
        store_->set_current_uuid("");
        if (client_) client_->clear_history();
        clear_messages();
        auto convos = store_->list();
        if (!convos.empty()) {
            open_conversation(convos.front().uuid, stay_view);
        } else {
            new_conversation(stay_view);
        }
    }
    request_list_rebuild();
    relayout_scroll_repaint();
}

void agent_tab::begin_rename(const std::string& uuid, const std::string& name) {
    if (uuid.empty()) return;
    rename_uuid_ = uuid;
    if (rename_dialog_) {
        rename_dialog_->show(i18n_manager::get().tr("chat.rename_title"),
                             i18n_manager::get().tr("chat.rename_hint"), name,
                             [this](const std::string& s) { commit_rename_text(s); },
                             [this]() { rename_uuid_.clear(); });
    }
}

void agent_tab::commit_rename_text(const std::string& text) {
    if (!store_ || rename_uuid_.empty()) return;
    std::string new_name = text;
    size_t b = new_name.find_first_not_of(" \t\r\n");
    size_t e = new_name.find_last_not_of(" \t\r\n");
    if (b == std::string::npos) new_name.clear();
    else new_name = new_name.substr(b, e - b + 1);
    if (!new_name.empty()) {
        chat_archive archive;
        if (store_->load(rename_uuid_, archive)) {
            archive.name = new_name;
            store_->save(archive);
        }
    }
    rename_uuid_.clear();
    request_list_rebuild();
    relayout_scroll_repaint();
}

void agent_tab::request_list_rebuild() {
    list_rebuild_pending_ = true;
}

void agent_tab::rebuild_conversation_list(const std::vector<conversation_meta>* preset) {
    if (!list_scroll_) return;
    while (!list_scroll_->children().empty())
        list_scroll_->remove_child(list_scroll_->children().front().get());

    std::vector<conversation_meta> convos =
        preset ? *preset : (store_ ? store_->list() : std::vector<conversation_meta>{});
    for (const auto& c : convos) {
        bool active = (c.uuid == current_uuid_);

        auto row = std::make_unique<container>();
        row->set_layout_manager(std::make_unique<horizontal_layout>(0.0f));
        row->widget_style.height = 36.0f;
        row->height = 36.0f;
        row->widget_style.background_color = color::transparent();

        auto open_btn = std::make_unique<button>();
        open_btn->text = c.name;
        open_btn->h_align = text_alignment::left;
        open_btn->widget_style.padding = margin(10, 0);
        open_btn->ellipsize = true;
        open_btn->set_base_bg(active ? theme_manager::get(theme_manager::BUTTON_HOVER)
                                     : color::transparent());
        open_btn->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
        open_btn->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
        open_btn->on_click = [this, uuid = c.uuid]() { open_conversation(uuid); };
        button* open_raw = open_btn.get();
        open_raw->on_right_click = [this, uuid = c.uuid, name = c.name, open_raw](float mx, float my) {
            auto menu = std::make_unique<context_menu>();
            menu->add_item(i18n_manager::get().tr("chat.rename"),
                           [this, uuid, name]() { begin_rename(uuid, name); });
            menu->add_item(i18n_manager::get().tr("chat.delete"),
                           [this, uuid]() { delete_conversation(uuid); });
            point sp = open_raw->to_screen(mx, my);
            request_context_menu(sp.x, sp.y, std::move(menu));
        };
        row->add_child(std::move(open_btn));

        list_scroll_->add_child(std::move(row));
    }
    list_scroll_->layout();
    if (request_repaint_) request_repaint_();
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

void agent_tab::begin_run() {
    if (!client_) {
        update_send_button();
        if (request_repaint_) request_repaint_();
        return;
    }

    client_->set_reasoning_level(reasoning_);
    thinking_capsule_ = nullptr;
    thinking_scroll_ = nullptr;
    thinking_label_ = nullptr;
    waiting_ = true;
    stop_requested_ = false;
    supplement_requested_ = false;
    streamed_content_ = false;
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
        ev.name = tc.function_name;
        ev.args = tc.arguments;
        stream_events_.push_back(std::move(ev));
    };
    events.on_tool_result = [this, alive](const tool_execution& ex) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_event ev;
        ev.t = stream_event::type::tool_result;
        ev.text = ex.result;
        stream_events_.push_back(std::move(ev));
    };
    events.should_stop = [this, alive]() {
        return !alive->load() || stop_requested_.load();
    };
    events.should_yield = [this, alive]() {
        return !alive->load() || supplement_requested_.load();
    };
    events.on_approve = [this, alive](const tool_call& tc) -> bool {
        if (!alive->load()) return false;
        return approve_request(tc);
    };

    pending_ = std::async(std::launch::async, [this, events, alive]() {
        if (!alive->load()) return chat_response{};
        return client_->run(100, events);
    });
    if (request_repaint_) request_repaint_();
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
    client_->add_message({"user", text, "", "", {}});
    begin_run();
}

void agent_tab::continue_generation() {
    if (waiting_ || !client_) return;
    begin_run();
}

void agent_tab::add_continue_button() {
    if (continue_btn_) return;

    auto row = std::make_unique<container>();
    row->set_layout_manager(std::make_unique<horizontal_layout>(0.0f));
    row->widget_style.height = 28.0f;
    row->widget_style.background_color = color::transparent();

    auto btn = std::make_unique<button>();
    btn->text = i18n_manager::get().tr("chat.continue");
    btn->widget_style.width = 80;
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

void agent_tab::update_token_label() {
    if (!token_label_ || !client_) return;
    token_label_->text = "\xE2\x86\x91 " + std::to_string(client_->input_tokens()) +
                         "  \xE2\x86\x93 " + std::to_string(client_->output_tokens());
    token_label_->width = std::max(24.0f, std::min(
        96.0f, token_text_estimate_width(token_label_) + 10.0f));
    relayout_scroll_repaint();
}

void agent_tab::set_reasoning_level(reasoning_level l) {
    reasoning_ = l;
    if (client_) client_->set_reasoning_level(l);
    if (reasoning_combo_) {
        reasoning_combo_->selected_index =
            (l == reasoning_level::none) ? 0 : (l == reasoning_level::deep) ? 2 : 1;
    }
    if (request_repaint_) request_repaint_();
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

collapsible* agent_tab::append_tool_capsule(const std::string& summary, const std::string& args) {
    tool_capsule_ = create_capsule(summary);
    tool_capsule_scroll_ = static_cast<container*>(tool_capsule_->content());

    auto md = std::make_unique<markdown>();
    md->font_size = 12.0f;
    md->selectable = true;
    md->text = tool_block_markdown(i18n_manager::get().tr("chat.tool_input", "输入"), args);
    tool_label_ = md.get();
    tool_capsule_scroll_->add_child(std::move(md));
    return tool_capsule_;
}

void agent_tab::append_tool_result(const std::string& text) {
    if (tool_label_) {
        tool_label_->text += tool_block_markdown(
            i18n_manager::get().tr("chat.tool_output", "输出"), text);
        tool_label_->layout();
    }
    tool_capsule_ = nullptr;
    tool_capsule_scroll_ = nullptr;
    tool_label_ = nullptr;
    relayout_scroll_repaint();
}

void agent_tab::restore_history_ui(const std::vector<chat_message>& msgs) {
    layout_batch_ = true;
    std::vector<container*> pending_output;
    for (const auto& m : msgs) {
        if (m.role == "user") {
            add_message("user", m.content);
        } else if (m.role == "assistant") {
            if (!m.reasoning_content.empty()) {
                thinking_capsule_ = create_capsule(i18n_manager::get().tr("chat.thinking"));
                thinking_scroll_ = static_cast<container*>(thinking_capsule_->content());
                auto md = std::make_unique<markdown>();
                md->font_size = 12.0f;
                md->selectable = true;
                md->text = m.reasoning_content;
                thinking_label_ = md.get();
                thinking_scroll_->add_child(std::move(md));
            }
            if (!m.content.empty())
                add_message("assistant", m.content);
            for (const auto& tc : m.tool_calls) {
                auto* cap = append_tool_capsule(tool_display_name(tc.function_name), tc.arguments);
                if (cap) pending_output.push_back(static_cast<container*>(cap->content()));
            }
        } else if (m.role == "tool") {
            if (!pending_output.empty()) {
                container* sc = pending_output.front();
                pending_output.erase(pending_output.begin());
                if (sc) {
                    auto md = std::make_unique<markdown>();
                    md->font_size = 12.0f;
                    md->selectable = true;
                    md->text = tool_block_markdown(
                        i18n_manager::get().tr("chat.tool_output", "输出"), m.content);
                    sc->add_child(std::move(md));
                }
            }
        }
    }
    thinking_capsule_ = nullptr;
    thinking_scroll_ = nullptr;
    thinking_label_ = nullptr;
    tool_capsule_ = nullptr;
    tool_capsule_scroll_ = nullptr;
    tool_label_ = nullptr;
    layout_batch_ = false;
    relayout_scroll_repaint();
}

void agent_tab::show_inline_approval() {
    if (!tool_capsule_ || !tool_capsule_scroll_ || approval_inline_shown_) return;
    approval_inline_shown_ = true;
    tool_capsule_->set_expanded(true);

    auto hint = std::make_unique<label>();
    hint->text = i18n_manager::get().tr("chat.approve_message");
    hint->font_size = 12.0f;
    approval_hint_ = hint.get();
    tool_capsule_scroll_->add_child(std::move(hint));

    auto row = std::make_unique<container>();
    row->set_layout_manager(std::make_unique<horizontal_layout>(8.0f));
    row->widget_style.background_color = color::transparent();
    row->widget_style.padding = margin(0, 4);
    row->widget_style.height = 24;
    row->height = 24;

    auto allow = std::make_unique<button>();
    allow->text = i18n_manager::get().tr("chat.approve");
    allow->widget_style.width = 58;
    allow->widget_style.height = 24;
    allow->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    allow->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    allow->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    allow->on_click = [this]() { resolve_approval(true); };
    approval_allow_btn_ = allow.get();
    row->add_child(std::move(allow));

    auto deny = std::make_unique<button>();
    deny->text = i18n_manager::get().tr("chat.deny");
    deny->widget_style.width = 58;
    deny->widget_style.height = 24;
    deny->hover_color = theme_manager::get(theme_manager::CLOSE_HOVER);
    deny->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    deny->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    deny->on_click = [this]() { resolve_approval(false); };
    approval_deny_btn_ = deny.get();
    row->add_child(std::move(deny));

    approval_row_ = row.get();
    tool_capsule_scroll_->add_child(std::move(row));
    relayout_scroll_repaint();
    if (scroll_) scroll_->scroll_to_y(99999.0f);
}

void agent_tab::remove_inline_approval() {
    if (!approval_inline_shown_) return;
    approval_inline_shown_ = false;
    if (tool_capsule_scroll_) {
        if (approval_row_) tool_capsule_scroll_->remove_child(approval_row_);
        if (approval_hint_) tool_capsule_scroll_->remove_child(approval_hint_);
    }
    approval_row_ = nullptr;
    approval_hint_ = nullptr;
    approval_allow_btn_ = nullptr;
    approval_deny_btn_ = nullptr;
    relayout_scroll_repaint();
}

bool agent_tab::approve_request(const tool_call& tc) {
    if (auto_approve_on_.load()) return true;

    auto promise = std::make_shared<std::promise<bool>>();
    {
        std::lock_guard<std::mutex> lk(approval_mtx_);
        if (approval_pending_.load()) return false;
        approval_tc_ = tc;
        approval_promise_ = promise;
        approval_pending_ = true;
    }
    if (request_repaint_) request_repaint_();

    auto fut = promise->get_future();
    while (true) {
        if (stop_requested_.load() || !alive_->load()) {
            resolve_approval(false);
            return false;
        }
        if (fut.wait_for(std::chrono::milliseconds(50)) == std::future_status::ready)
            return fut.get();
    }
}

void agent_tab::resolve_approval(bool ok) {
    std::shared_ptr<std::promise<bool>> p;
    {
        std::lock_guard<std::mutex> lk(approval_mtx_);
        if (!approval_pending_.load()) return;
        p = approval_promise_;
        approval_promise_.reset();
        approval_pending_ = false;
    }
    if (p) p->set_value(ok);
    if (request_repaint_) request_repaint_();
}

void agent_tab::tick(float dt_ms) {
    if (initial_load_.valid()) {
        auto st = initial_load_.wait_for(std::chrono::milliseconds(0));
        if (st == std::future_status::ready) {
            initial_load_result res = initial_load_.get();
            apply_initial_load(res);
        }
    }

    if (list_rebuild_pending_) {
        list_rebuild_pending_ = false;
        rebuild_conversation_list();
    }

    if (rename_dialog_) rename_dialog_->tick(dt_ms);

    if (tokens_dirty_.exchange(false)) update_token_label();

    if (approval_pending_.load()) {
        if (!approval_inline_shown_ && tool_capsule_ && tool_capsule_scroll_)
            show_inline_approval();
    } else if (approval_inline_shown_) {
        remove_inline_approval();
    }

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
                update_token_label();
                save_current();
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
            } else if (!resp.content.empty() && !stream_label_ && !streamed_content_) {
                add_message("assistant", resp.content);
            }

            stream_label_ = nullptr;

            if (stop_requested_.load() && queued_inputs_.empty()) {
                add_continue_button();
            }

            update_token_label();
            save_current();

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
