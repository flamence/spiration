/**
 * @file agent_tab.cpp
 * @brief 智能体标签页实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/builtin/agent/agent_tab.h>
#include <spiration/extension/builtin/agent/tool/terminal_tool.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/utils/dispatcher.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <fmt/format.h>
#include <tl/optional.hpp>

namespace spiration {
namespace agent {

constexpr float agent_tab::MAX_INPUT_H;

namespace {

constexpr float SETTINGS_H = 34.0f;   ///< 底部设置栏高度
constexpr float INPUTBAR_H = 46.0f;   ///< 输入栏高度
constexpr float PAD        = 12.0f;   ///< 输入栏外边距

/// @brief 返回字符串末尾不完整 UTF-8 序列的字节数（0 表示以完整字符结尾）。
size_t trailing_incomplete_utf8(const std::string& s) {
    const size_t n = s.size();
    if (n == 0) return 0;
    const unsigned char last = static_cast<unsigned char>(s[n - 1]);
    if (last < 0x80) return 0;
    size_t lead_index = n - 1;
    while (lead_index > 0 &&
           (static_cast<unsigned char>(s[lead_index]) & 0xC0) == 0x80) {
        --lead_index;
    }
    const unsigned char lead = static_cast<unsigned char>(s[lead_index]);
    size_t total = 0;
    if ((lead & 0xF8) == 0xF0) total = 4;
    else if ((lead & 0xF0) == 0xE0) total = 3;
    else if ((lead & 0xE0) == 0xC0) total = 2;
    else return 0;
    const size_t have = n - lead_index;
    return (have < total) ? have : 0;
}

/// @brief 把 src 追加到 dest，末尾不完整的 UTF-8 字节暂存到 hold 供下批补全。
void utf8_append(std::string& dest, std::string& hold, const std::string& src) {
    std::string add = hold + src;
    hold.clear();
    const size_t hold_bytes = trailing_incomplete_utf8(add);
    if (hold_bytes > 0) {
        hold = add.substr(add.size() - hold_bytes);
        add.resize(add.size() - hold_bytes);
    }
    dest += add;
}

std::string tool_display_name(const std::string& name) {
    return i18n_manager::get().tr("agent:tool." + name, name);
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

/// @brief 取首行并截断到最多 max_bytes 个完整 UTF-8 字节。
std::string first_line_clipped(const std::string& text, size_t max_bytes) {
    std::string line = text.substr(0, text.find('\n'));
    size_t b = line.find_first_not_of(" \t\r");
    size_t e = line.find_last_not_of(" \t\r");
    if (b == std::string::npos) return {};
    line = line.substr(b, e - b + 1);
    if (line.size() <= max_bytes) return line;
    size_t cut = max_bytes;
    while (cut > 0 && (static_cast<unsigned char>(line[cut]) & 0xC0) == 0x80) --cut;
    return line.substr(0, cut);
}

} // namespace

agent_tab::agent_tab(std::shared_ptr<chat_client> client,
                     std::shared_ptr<chat_store> store,
                     std::string conversation_uuid,
                     std::string conversation_name)
    : store_(std::move(store))
    , client_(std::move(client))
    , conversation_uuid_(std::move(conversation_uuid))
    , name_(std::move(conversation_name)) {
    auto_title_ = name_.empty();
    title_ = auto_title_ ? i18n_manager::get().tr("agent:chat.new_chat") : name_;
    widget_style.background_color = theme_manager::get(theme_manager::CONTENT_BG);

    if (client_) {
        client_->on_tokens_updated = [this, alive = alive_]() {
            if (alive->load()) tokens_dirty_.store(true);
        };
        reasoning_ = client_->get_config().reasoning;
    }

    auto sv = std::make_unique<container>();
    sv->widget_style.overflow_y = true;
    sv->widget_style.background_color = color::transparent();
    scroll_ = sv.get();
    add_child(std::move(sv));

    auto ml = std::make_unique<container>();
    ml->set_layout_manager(std::make_unique<vertical_layout>(8.0f));
    ml->widget_style.padding = margin(10, 12);
    ml->widget_style.background_color = color::transparent();
    msg_list_ = ml.get();
    scroll_->add_child(std::move(ml));

    auto tc = std::make_unique<collapsible>();
    tc->set_summary(i18n_manager::get().tr("agent:chat.todo"));
    tc->max_content_height = 200.0f;
    tc->set_expanded(false);
    tc->show_summary_action = true;
    todo_capsule_ = tc.get();

    auto tv = std::make_unique<todo_view>();
    tv->bind_uuid([this]() { return conversation_uuid_; });
    tv->on_content_changed = [this]() { update_todo_capsule(); };
    todo_view_ = tv.get();
    tc->on_summary_action = [this]() {
        todo_store::instance().clear(conversation_uuid_);
        update_todo_capsule();
        save_async();
    };
    tc->on_collapsed = [this]() {
        if (todo_count_ == 0 && todo_capsule_) {
            todo_capsule_->width = 0.0f;
            todo_capsule_->height = 0.0f;
            schedule_layout();
        }
    };
    tc->set_content(std::move(tv));
    add_child(std::move(tc));

    auto sb = std::make_unique<container>();
    sb->widget_style.background_color = theme_manager::get(theme_manager::CONTENT_BG);
    settings_bar_ = sb.get();
    add_child(std::move(sb));

    auto mc = std::make_unique<combo_box>();
    mc->font_size = 12.0f;
    mc->item_height = 22.0f;
    mc->popup_up = true;
    mc->on_changed = [this](int idx) {
        try {
            if (!client_ || !model_combo_) return;
            if (idx >= 0 && idx < static_cast<int>(models_.size())) {
                client_->configure(models_[static_cast<size_t>(idx)].cfg);
                if (on_config_changed) on_config_changed(client_->get_config());
                save_async();
                refresh_reasoning_combo();
            }
        } catch (const std::exception& e) {
            application::instance()->logger()->log({logging::level::error, fmt::format("model switch threw: {}", e.what()), "extension/agent"});
        }
    };
    model_combo_ = mc.get();
    settings_bar_->add_child(std::move(mc));

    auto rc = std::make_unique<combo_box>();
    rc->font_size = 12.0f;
    rc->item_height = 22.0f;
    rc->popup_up = true;
    rc->on_changed = [this](int idx) {
        auto levels = client_ ? client_->supported_reasoning_levels()
                              : std::vector<reasoning_level>{};
        if (idx >= 0 && idx < static_cast<int>(levels.size()))
            set_reasoning_level(levels[static_cast<size_t>(idx)]);
    };
    reasoning_combo_ = rc.get();
    settings_bar_->add_child(std::move(rc));
    refresh_reasoning_combo();

    auto ap = std::make_unique<checkbox>();
    ap->text = i18n_manager::get().tr("agent:chat.auto_approve");
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
    add_child(std::move(bar));

    auto ta = std::make_unique<text_area>();
    ta->placeholder = i18n_manager::get().tr("agent:tab.agent.input_placeholder");
    ta->font_size = 13.0f;
    ta->font_family = theme_manager::get_str(theme_manager::UI_FONT);
    ta->line_height = 20.0f;
    ta->text_pad_top = 7.0f;
    ta->text_pad_left = 8.0f;
    ta->text_pad_right = 10.0f;
    ta->show_line_numbers = false;
    ta->submit_on_enter = true;
    ta->widget_style.background_color = theme_manager::get(theme_manager::INPUT_BG);
    ta->widget_style.margin = margin(6, 0);
    ta->on_submit = [this](const std::string&) { send(); };
    ta->on_changed = [this](const std::string&) { update_input_height(); };
    input_ = ta.get();
    input_bar_->add_child(std::move(ta));

    auto btn = std::make_unique<button>();
    btn->text = i18n_manager::get().tr("agent:tab.agent.send");
    btn->widget_style.width = 56;
    btn->widget_style.margin = margin(6, 0);
    btn->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    btn->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    btn->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    btn->on_click = [this]() {
        if (waiting_) {
            stop_flag_->store(true);
            queued_inputs_.clear();
            supplement_requested_ = false;
        } else {
            send();
        }
    };
    send_btn_ = btn.get();
    input_bar_->add_child(std::move(btn));

    auto rd = std::make_unique<input_dialog>();
    rename_dialog_ = rd.get();
    add_child(std::move(rd));

    if (client_ && !client_->get_config().model.empty()) {
        model_combo_->items = {client_->get_config().model};
        model_combo_->selected_index = 0;
    }

    if (store_ && !conversation_uuid_.empty()) {
        initial_load_ = std::async(std::launch::async, [store = store_, uuid = conversation_uuid_]() {
            initial_load_result res;
            res.loaded = store->load(uuid, res.archive);
            return res;
        });
    }
}

agent_tab::~agent_tab() {
    *alive_ = false;
    stop_flag_->store(true);
    if (pending_.valid()) pending_.wait();
    if (initial_load_.valid()) initial_load_.wait();
    if (!deleted_ && store_ && !conversation_uuid_.empty()) {
        // 从未产生内容的空对话不留在历史里。
        if (messages_.empty() && !continue_btn_) {
            store_->remove(conversation_uuid_);
            todo_store::instance().remove(conversation_uuid_);
        } else {
            save_now();
        }
    }
    if (on_destroyed) on_destroyed();
}

void agent_tab::wait_initial_load() {
    if (initial_load_.valid()) initial_load_.wait();
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

void agent_tab::schedule_layout() {
    if (layout_batch_) return;
    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

void agent_tab::schedule_scroll_bottom() {
    if (layout_batch_) return;
    scroll_bottom_pending_ = true;
    schedule_layout();
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

    if (type == event_type::keyboard) {
        auto* kd = static_cast<key_event_data*>(data);
        if (input_) input_->handle_event(type, data);
        if (!kd->consumed && msg_list_) msg_list_->handle_event(type, data);
        return;
    }
    if (type == event_type::ime_composition) {
        if (input_) input_->handle_event(type, data);
        return;
    }

    widget::handle_event(type, data);
}

void agent_tab::layout() {
    on_layout_begin();

    float input_h = INPUTBAR_H;
    if (input_) {
        input_h = std::max(INPUTBAR_H, std::min(input_->content_preferred_height(), MAX_INPUT_H));
    }

    const float settings_y = height - SETTINGS_H - input_h - PAD;
    float capsule_h = 0.0f;
    if (todo_count_ > 0) {
        todo_capsule_->x = 0;
        todo_capsule_->width = width;
        if (todo_capsule_->needs_layout()) todo_capsule_->layout();
        capsule_h = todo_capsule_->height;
        todo_capsule_->y = settings_y - capsule_h;
        todo_capsule_->finish_layout_geometry();
    } else if (todo_capsule_->width > 0.0f) {
        todo_capsule_->width = width;
        if (todo_capsule_->needs_layout()) todo_capsule_->layout();
        capsule_h = todo_capsule_->height;
        todo_capsule_->y = settings_y - capsule_h;
        todo_capsule_->finish_layout_geometry();
    } else {
        todo_capsule_->width = 0.0f;
        todo_capsule_->height = 0.0f;
    }
    const float capsule_y = settings_y - capsule_h;

    scroll_->x = 0;
    scroll_->y = 0;
    scroll_->width = width;
    scroll_->height = std::max(0.0f, capsule_y);
    if (scroll_->needs_layout()) scroll_->layout();

    settings_bar_->x = 0;
    settings_bar_->y = settings_y;
    settings_bar_->width = width;
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
        token_label_->x = std::max(0.0f, width - tw - GAP);
        token_label_->y = 2.0f;
        token_label_->width = tw;
        token_label_->height = SETTINGS_H - 4.0f;
        token_label_->layout();
    }

    float model_w = MODEL_W, reason_w = REASON_W;
    float token_left = token_label_ ? token_label_->x : width;
    float left_end = GAP + AUTO_W + GAP + model_w + GAP + reason_w;
    float overlap = left_end - token_left;
    if (overlap > 0.0f) {
        float reduce = std::min(overlap, (REASON_W - MIN_COMBO_W) + (MODEL_W - MIN_COMBO_W));
        float dr = std::min(reduce, REASON_W - MIN_COMBO_W);
        reason_w -= dr;
        reduce -= dr;
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

    input_bar_->x = PAD;
    input_bar_->y = height - input_h - PAD;
    input_bar_->width = std::max(0.0f, width - PAD * 2);
    input_bar_->height = input_h;
    if (input_bar_->needs_layout()) input_bar_->layout();

    if (rename_dialog_) {
        rename_dialog_->x = 0;
        rename_dialog_->y = 0;
        rename_dialog_->width = width;
        rename_dialog_->height = height;
        if (rename_dialog_->needs_layout()) rename_dialog_->layout();
    }

    widget::layout();
}

void agent_tab::on_activate() {
    schedule_layout();
    schedule_scroll_bottom();
    focus_input_pending_ = true;
}

widget* agent_tab::hit_test_hover(float x, float y) const {
    if (rename_dialog_ && rename_dialog_->visible())
        return rename_dialog_->hit_test_hover(x, y);
    return widget::hit_test_hover(x, y);
}

void agent_tab::add_message(const std::string& role, const std::string& content) {
    messages_.push_back({role, content});
    last_label_ = append_bubble(messages_.back());
}

markdown* agent_tab::append_bubble(const display_message& msg) {
    const bool is_user = (msg.role == "user");

    auto md = std::make_unique<markdown>();
    md->text = msg.content;
    md->font_size = 13.0f;
    md->selectable = true;
    md->h_align = is_user ? text_alignment::right : text_alignment::left;
    md->v_align = vertical_alignment::top;

    markdown* raw = md.get();
    msg_list_->add_child(std::move(md));
    schedule_scroll_bottom();
    return raw;
}

void agent_tab::clear_messages() {
    messages_.clear();
    if (msg_list_) msg_list_->clear_children();
    stream_label_ = nullptr;
    last_label_ = nullptr;
    thinking_capsule_ = nullptr;
    thinking_scroll_ = nullptr;
    thinking_label_ = nullptr;
    tool_capsule_ = nullptr;
    tool_capsule_scroll_ = nullptr;
    tool_label_ = nullptr;
    continue_btn_ = nullptr;
    invalidate_layout();
    if (request_repaint_) request_repaint_();
}

void agent_tab::apply_initial_load(const initial_load_result& res) {
    if (!store_ || !res.loaded) return;
    const chat_archive& archive = res.archive;
    created_at_ = archive.created_at;
    if (name_.empty() && !archive.name.empty()) name_ = archive.name;
    if (!auto_title_) set_title(name_);

    if (client_) {
        if (!archive.model.empty() &&
            (archive.model != client_->get_config().model ||
             (!archive.provider.empty() && archive.provider != client_->get_config().provider))) {
            chat_client::config c = client_->get_config();
            c.model = archive.model;
            if (!archive.provider.empty()) c.provider = archive.provider;
            client_->configure(c);
        }
        client_->clear_history();
        for (const auto& m : archive.messages) {
            if (m.role == "system") continue;
            client_->add_message(m);
        }
        client_->set_tokens(archive.tokens_in, archive.tokens_out);
    }
    auto_approve_on_ = archive.auto_approve;
    if (auto_approve_) auto_approve_->checked = archive.auto_approve;
    todo_store::instance().set(conversation_uuid_, archive.todos);
    restore_history_ui(archive.messages);
    if (archive.can_continue) add_continue_button();
    update_token_label();
    update_todo_capsule();
    schedule_scroll_bottom();
}

void agent_tab::build_archive(chat_archive& archive) const {
    chat_archive existing;
    if (store_) store_->load(conversation_uuid_, existing);
    archive.name       = name_.empty() ? existing.name : name_;
    archive.created_at = created_at_.empty() ? existing.created_at : created_at_;
    archive.memory     = existing.memory;
    archive.uuid       = conversation_uuid_;
    if (client_) {
        archive.provider  = client_->provider_name();
        archive.model     = client_->get_config().model;
        archive.messages  = client_->history();
        archive.tokens_in = client_->input_tokens();
        archive.tokens_out = client_->output_tokens();
    }
#if !defined(__OHOS__)
    archive.terminals = terminal_manager::instance().snapshots(conversation_uuid_);
#endif
    archive.todos      = todo_store::instance().items(conversation_uuid_);
    archive.can_continue = can_continue();
    archive.auto_approve = auto_approve_on_.load();
}

void agent_tab::save_async() {
    if (!store_ || conversation_uuid_.empty()) return;
    auto archive = std::make_shared<chat_archive>();
    build_archive(*archive);
    if (auto* d = application::instance()->dispatcher()) {
        d->post([store = store_, archive]() { store->save(*archive); });
    } else {
        store_->save(*archive);
    }
}

void agent_tab::save_now() {
    if (!store_ || conversation_uuid_.empty()) return;
    chat_archive archive;
    build_archive(archive);
    store_->save(archive);
}

void agent_tab::auto_name_from(const std::string& text) {
    std::string line = first_line_clipped(text, 32);
    if (line.empty()) return;
    name_ = line;
    auto_title_ = false;
    set_title(name_);
}

void agent_tab::update_input_height() {
    schedule_layout();
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

    bool at_bottom = true;
    if (scroll_ && scroll_->scroll_max_y() > 0.0f) {
        at_bottom = scroll_->scroll_offset_y() >= scroll_->scroll_max_y() - 1.0f;
    }

    bool need_layout = false;

    for (size_t k = 0; k < events.size();) {
        stream_event& ev = events[k];
        if (ev.t == stream_event::type::delta || ev.t == stream_event::type::reasoning) {
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
                    thinking_capsule_ = create_capsule(i18n_manager::get().tr("agent:chat.thinking"));
                    thinking_scroll_ = static_cast<container*>(thinking_capsule_->content());
                    auto md = std::make_unique<markdown>();
                    md->font_size = 12.0f;
                    md->selectable = true;
                    thinking_label_ = md.get();
                    thinking_scroll_->add_child(std::move(md));
                    thinking_capsule_->set_expanded(true);
                    need_layout = true;
                }
                if (thinking_label_) {
                    utf8_append(thinking_label_->text, utf8_hold_reasoning_, batch);
                    thinking_label_->invalidate_layout();
                    need_layout = true;
                }
            } else {
                if (!stream_label_) {
                    add_message("assistant", "");
                    stream_label_ = last_label_;
                    stream_msg_index_ = messages_.size() - 1;
                    need_layout = true;
                }
                streamed_content_ = true;
                utf8_append(stream_label_->text, utf8_hold_delta_, batch);
                if (stream_msg_index_ < messages_.size())
                    messages_[stream_msg_index_].content = stream_label_->text;
                stream_label_->invalidate_layout();
                need_layout = true;
            }
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
                if (tool_capsule_) tool_capsule_->set_expanded(true);
                need_layout = true;
                break;
            case stream_event::type::tool_result:
                stream_label_ = nullptr;
                append_tool_result(ev.text.empty() ? i18n_manager::get().tr("agent:chat.no_output")
                                                   : ev.text);
                need_layout = true;
                break;
            default:
                break;
        }
        ++k;
    }

    if (need_layout) {
        if (at_bottom) schedule_scroll_bottom();
        schedule_layout();
    }
}

void agent_tab::send() {
    if (!input_ || input_->text.empty()) return;
    std::string text = input_->text;
    input_->text.clear();
    input_->reset_view();
    update_input_height();

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
    stop_flag_->store(false);
    utf8_hold_reasoning_.clear();
    utf8_hold_delta_.clear();
    supplement_requested_ = false;
    streamed_content_ = false;
    remove_continue_button();
    update_send_button();

    auto alive = alive_;
    chat_events events;
    events.on_delta = [this, alive](const std::string& delta) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_events_.push_back({stream_event::type::delta, delta, "", ""});
    };
    events.on_reasoning_delta = [this, alive](const std::string& delta) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_events_.push_back({stream_event::type::reasoning, delta, "", ""});
    };
    events.on_tool_call = [this, alive](const tool_call& tc) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_events_.push_back({stream_event::type::tool_call, tc.function_name, tc.function_name, tc.arguments});
    };
    events.on_tool_result = [this, alive](const tool_execution& ex) {
        if (!alive->load()) return;
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_events_.push_back({stream_event::type::tool_result, ex.result, "", ""});
    };
    events.should_stop = [alive, stop = stop_flag_]() {
        return !alive->load() || stop->load();
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
        return client_->run(events, tl::nullopt);
    });
    if (request_repaint_) request_repaint_();
}

void agent_tab::start_send(const std::string& text, bool show_user) {
    if (auto_title_ && show_user) auto_name_from(text);
    if (show_user) add_message("user", text);
    if (!client_) {
        add_message("assistant", i18n_manager::get().tr("agent:chat.echo") + " " + text);
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
    btn->text = i18n_manager::get().tr("agent:chat.continue");
    btn->widget_style.width = 80;
    btn->widget_style.margin = margin(0, 0);
    btn->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    btn->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    btn->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    btn->on_click = [this]() { continue_generation(); };
    continue_btn_ = btn.get();
    row->add_child(std::move(btn));
    msg_list_->add_child(std::move(row));
    schedule_scroll_bottom();
}

void agent_tab::remove_continue_button() {
    if (continue_btn_ && msg_list_) {
        if (auto* p = continue_btn_->parent()) msg_list_->remove_child(p);
    }
    continue_btn_ = nullptr;
}

void agent_tab::update_send_button() {
    if (!send_btn_) return;
    send_btn_->text = waiting_
        ? i18n_manager::get().tr("agent:chat.stop")
        : i18n_manager::get().tr("agent:tab.agent.send");
}

void agent_tab::update_token_label() {
    if (!token_label_ || !client_) return;
    token_label_->text = "\xE2\x86\x91 " + std::to_string(client_->input_tokens()) +
                         "  \xE2\x86\x93 " + std::to_string(client_->output_tokens());
    token_label_->width = std::max(24.0f, std::min(
        96.0f, token_text_estimate_width(token_label_) + 10.0f));
    schedule_layout();
}

void agent_tab::set_reasoning_level(reasoning_level l) {
    reasoning_ = l;
    if (client_) client_->set_reasoning_level(l);
    refresh_reasoning_combo();
    if (request_repaint_) request_repaint_();
}

void agent_tab::refresh_reasoning_combo() {
    if (!reasoning_combo_) return;
    std::vector<reasoning_level> levels;
    try {
        levels = client_ ? client_->supported_reasoning_levels()
                         : std::vector<reasoning_level>{};
    } catch (const std::exception& e) {
        application::instance()->logger()->log({logging::level::error, fmt::format("supported_reasoning_levels threw: {}", e.what()), "extension/agent"});
        return;
    }
    if (levels.empty()) return;
    reasoning_combo_->items.clear();
    for (auto l : levels) {
        std::string key = "agent:reasoning." + std::string(reasoning_level_to_string(l));
        reasoning_combo_->items.push_back(i18n_manager::get().tr(key, key));
    }
    reasoning_combo_->selected_index = -1;
    for (size_t i = 0; i < levels.size(); ++i) {
        if (levels[i] == reasoning_) {
            reasoning_combo_->selected_index = static_cast<int>(i);
            break;
        }
    }
    if (reasoning_combo_->selected_index < 0)
        reasoning_combo_->selected_index = 0;
}

void agent_tab::update_todo_capsule() {
    if (!todo_capsule_) return;
    auto items = todo_store::instance().items(conversation_uuid_);
    todo_count_ = items.size();
    size_t done = 0;
    for (const auto& it : items) {
        if (it.status == todo_status::completed) ++done;
    }
    std::string summary = i18n_manager::get().tr("agent:chat.todo");
    if (todo_count_ > 0)
        summary += " (" + std::to_string(done) + "/" + std::to_string(todo_count_) + ")";
    todo_capsule_->set_summary(summary);

    const bool now_has = (todo_count_ > 0);
    if (now_has && !todo_has_) {
        todo_capsule_->set_expanded(true);
    } else if (!now_has && todo_has_) {
        todo_capsule_->set_expanded(false);
    }
    todo_has_ = now_has;
    todo_capsule_->show_summary_action = now_has;
    schedule_layout();
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
    schedule_scroll_bottom();
    return raw;
}

collapsible* agent_tab::append_tool_capsule(const std::string& summary, const std::string& args) {
    tool_capsule_ = create_capsule(summary);
    tool_capsule_scroll_ = static_cast<container*>(tool_capsule_->content());

    auto md = std::make_unique<markdown>();
    md->font_size = 12.0f;
    md->selectable = true;
    md->text = tool_block_markdown(i18n_manager::get().tr("agent:chat.tool_input", "输入"), args);
    tool_label_ = md.get();
    tool_capsule_scroll_->add_child(std::move(md));
    return tool_capsule_;
}

void agent_tab::append_tool_result(const std::string& text) {
    if (tool_label_) {
        tool_label_->text += tool_block_markdown(
            i18n_manager::get().tr("agent:chat.tool_output", "输出"), text);
        tool_label_->invalidate_layout();
        if (tool_capsule_) tool_capsule_->invalidate_layout();
    }
    tool_capsule_ = nullptr;
    tool_capsule_scroll_ = nullptr;
    tool_label_ = nullptr;
    schedule_scroll_bottom();
}

void agent_tab::restore_history_ui(const std::vector<chat_message>& msgs) {
    layout_batch_ = true;
    std::vector<container*> pending_output;
    for (const auto& m : msgs) {
        if (m.role == "user") {
            add_message("user", m.content.value());
        } else if (m.role == "assistant") {
            if (m.reasoning_content.has_value()) {
                thinking_capsule_ = create_capsule(i18n_manager::get().tr("agent:chat.thinking"));
                thinking_scroll_ = static_cast<container*>(thinking_capsule_->content());
                auto md = std::make_unique<markdown>();
                md->font_size = 12.0f;
                md->selectable = true;
                md->text = m.reasoning_content.value();
                thinking_label_ = md.get();
                thinking_scroll_->add_child(std::move(md));
            }
            if (m.content.has_value())
                add_message("assistant", m.content.value());
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
                        i18n_manager::get().tr("agent:chat.tool_output", "输出"), m.content.value());
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
    schedule_layout();
}

void agent_tab::show_inline_approval() {
    if (!tool_capsule_ || !tool_capsule_scroll_ || approval_inline_shown_) return;
    approval_inline_shown_ = true;
    tool_capsule_->set_expanded(true);

    auto hint = std::make_unique<label>();
    hint->text = i18n_manager::get().tr("agent:chat.approve_message");
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
    allow->text = i18n_manager::get().tr("agent:chat.approve");
    allow->widget_style.width = 58;
    allow->widget_style.height = 24;
    allow->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
    allow->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
    allow->set_base_bg(theme_manager::get(theme_manager::BUTTON_BG));
    allow->on_click = [this]() { resolve_approval(true); };
    approval_allow_btn_ = allow.get();
    row->add_child(std::move(allow));

    auto deny = std::make_unique<button>();
    deny->text = i18n_manager::get().tr("agent:chat.deny");
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
    schedule_layout();
    schedule_scroll_bottom();
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
    schedule_layout();
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
        if (stop_flag_->load() || !alive_->load()) {
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

void agent_tab::begin_rename() {
    if (!rename_dialog_) return;
    rename_dialog_->show(i18n_manager::get().tr("agent:chat.rename_title"),
                         i18n_manager::get().tr("agent:chat.rename_hint"), name_,
                         [this](const std::string& s) {
                             std::string trimmed = s;
                             size_t b = trimmed.find_first_not_of(" \t\r\n");
                             size_t e = trimmed.find_last_not_of(" \t\r\n");
                             if (b == std::string::npos) return;
                             trimmed = trimmed.substr(b, e - b + 1);
                             if (trimmed.empty()) return;
                             name_ = trimmed;
                             auto_title_ = false;
                             set_title(name_);
                             save_async();
                             schedule_layout();
                         },
                         [this]() { schedule_layout(); });
    schedule_layout();
}

void agent_tab::begin_delete() {
    if (!rename_dialog_) return;
    rename_dialog_->show(i18n_manager::get().tr("agent:chat.delete_confirm"),
                         i18n_manager::get().tr("agent:chat.delete_confirm_hint"),
                         "",
                         [this](const std::string& s) {
                             std::string trimmed = s;
                             size_t b = trimmed.find_first_not_of(" \t\r\n");
                             size_t e = trimmed.find_last_not_of(" \t\r\n");
                             if (b == std::string::npos) return;
                             trimmed = trimmed.substr(b, e - b + 1);
                             if (trimmed == name_) delete_conversation();
                             schedule_layout();
                         },
                         [this]() { schedule_layout(); });
    schedule_layout();
}

void agent_tab::delete_conversation() {
    if (deleted_) return;
    deleted_ = true;
    const std::string uuid = conversation_uuid_;
    if (store_ && !uuid.empty()) store_->remove(uuid);
    todo_store::instance().remove(uuid);
#if !defined(__OHOS__)
    terminal_manager::instance().close_scope(uuid);
#endif
    if (on_delete_requested) on_delete_requested(this);
}

void agent_tab::tick(float dt_ms) {
    if (initial_load_.valid()) {
        auto st = initial_load_.wait_for(std::chrono::milliseconds(0));
        if (st == std::future_status::ready) {
            initial_load_result res = initial_load_.get();
            apply_initial_load(res);
        }
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
                add_message("assistant", i18n_manager::get().tr("agent:chat.request_failed") + " " + err);
                update_token_label();
                save_async();
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

            if (!utf8_hold_delta_.empty() && stream_label_) {
                stream_label_->text += utf8_hold_delta_;
                stream_label_->invalidate_layout();
            }
            utf8_hold_delta_.clear();
            if (!utf8_hold_reasoning_.empty() && thinking_label_) {
                thinking_label_->text += utf8_hold_reasoning_;
                thinking_label_->invalidate_layout();
            }
            utf8_hold_reasoning_.clear();

            process_stream_events();

            if (resp.content.has_value() && stream_label_) {
                stream_label_->text = resp.content.value();
                if (stream_msg_index_ < messages_.size())
                    messages_[stream_msg_index_].content = resp.content.value();
                stream_label_->invalidate_layout();
                schedule_scroll_bottom();
            } else if (resp.content.has_value() && !stream_label_ && !streamed_content_) {
                add_message("assistant", resp.content.value());
            }

            stream_label_ = nullptr;

            if (stop_flag_->load() && queued_inputs_.empty()) add_continue_button();

            update_token_label();
            save_async();

            if (!queued_inputs_.empty()) {
                std::string next = queued_inputs_.front();
                queued_inputs_.pop_front();
                start_send(next, false);
            }
            if (request_repaint_) request_repaint_();
        }
    }

    if (focus_input_pending_ && width > 0.0f) {
        focus_input_pending_ = false;
        if (input_ && !waiting_) input_->focus();
    }

    if (scroll_bottom_pending_) {
        const bool empty = msg_list_ && msg_list_->children().empty();
        if (empty) {
            scroll_bottom_pending_ = false;
        } else if (scroll_ && scroll_->scroll_max_y() > 0.0f) {
            scroll_->scroll_to_y(1e9f);
            scroll_bottom_pending_ = false;
        }
    }

    widget::tick(dt_ms);
}

} // namespace agent
} // namespace spiration
