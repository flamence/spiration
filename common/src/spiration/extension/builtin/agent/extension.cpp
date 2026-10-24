/**
 * @file extension.cpp
 * @brief 智能体拓展实现。
 * @author clk
 */

#include <spiration/application.h>
#include <spiration/extension/builtin/agent/extension.h>
#include <spiration/extension/builtin/agent/tool/edit_tool.h>
#include <spiration/extension/builtin/agent/tool/memory_tool.h>
#include <spiration/extension/builtin/agent/tool/sleep_tool.h>
#include <spiration/extension/builtin/agent/tool/terminal_tool.h>
#include <spiration/extension/builtin/agent/tool/todo_tool.h>
#include <spiration/extension/builtin/agent/tool/web_tool.h>
#include <spiration/extension/builtin/edit/edit_service.h>
#include <spiration/extension/extension_api.h>
#include <spiration/extension/extension_manager.h>
#include <spiration/ui/button.h>
#include <spiration/ui/context_menu.h>
#include <spiration/ui/root.h>
#include <spiration/ui/tab_bar.h>
#include <spiration/ui/theme_manager.h>
#include <spiration/utils/io.h>
#include <spiration/utils/platform.h>

#include <fmt/format.h>
#include <ghc/filesystem.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace spiration {
namespace agent {

namespace {

tab_bar* current_tab_bar() {
    auto* root = dynamic_cast<spiration::root*>(application::instance()->widget());
    return root ? root->get_tab_bar() : nullptr;
}

root* current_root() {
    return dynamic_cast<spiration::root*>(application::instance()->widget());
}

/// @brief 从单个模型 JSON 对象解析为 model_option。
model_option parse_model_json(const nlohmann::json& m, const std::string& key) {
    model_option opt;
    std::string model = m.value("model", "");
    opt.display_name = m.value("name", model.empty() ? key : model);
    opt.cfg.model    = model.empty() ? key : model;
    opt.cfg.endpoint = m.value("endpoint", opt.cfg.endpoint);
    opt.cfg.api_key  = m.value("api_key", opt.cfg.api_key);
    opt.cfg.provider = m.value("provider", opt.cfg.provider);
    opt.cfg.max_tokens = m.value("max_tokens", opt.cfg.max_tokens);
    opt.cfg.temperature = m.value("temperature", opt.cfg.temperature);
    opt.cfg.stream = m.value("stream", opt.cfg.stream);
    opt.cfg.timeout_seconds = m.value("timeout", opt.cfg.timeout_seconds);
    opt.cfg.reasoning = reasoning_level_from_string(m.value("reasoning", "medium"));
    return opt;
}

} // namespace

bool extension::initialize() {
    if (!api) return false;

    data_dir_ = api->extension_directory();
    ghc::filesystem::path models_path = data_dir_ / "cnf/models.json";
    if (ghc::filesystem::exists(models_path)) {
        std::string json = io::read_file_content(models_path);
        if (!json.empty()) {
            try {
                nlohmann::json j = nlohmann::json::parse(json);
                if (j.is_object()) {
                    for (auto& item : j.items()) {
                        if (item.value().is_object())
                            models_.push_back(parse_model_json(item.value(), item.key()));
                    }
                } else if (j.is_array()) {
                    for (auto& m : j) {
                        if (m.is_object()) models_.push_back(parse_model_json(m, ""));
                    }
                }
            } catch (const std::exception& e) {
                api->log_error(fmt::format("parse cnf/models.json failed: {}", e.what()));
            }
        }
    }
    if (!models_.empty()) default_cfg_ = models_.front().cfg;

    store_ = std::make_shared<chat_store>(data_dir_);

    api->add_menu_item("menu.help", i18n_manager::get().tr("agent:chat.new_chat"),
                       [this]() { open_new_chat(); });
    api->add_menu_item("menu.help", i18n_manager::get().tr("agent:menu.help.configure_models"),
                       [this]() { configure_models(); });

    application::instance()->extension()->register_service(
        id(), agent_registry::SERVICE_NAME, &agent_registry::instance());

    models_subscription_ = agent_registry::instance().on_models_changed(
        [this]() { on_models_changed(); });

    // 标签栏：历史按钮 + “新建对话” + 标签右键菜单。
    if (auto* tb = current_tab_bar()) {
        auto hist = std::make_unique<button>();
        hist->text = "\xE2\x98\xB0";  // ☰
        hist->h_align = text_alignment::center;
        hist->set_base_bg(color::transparent());
        hist->hover_color = theme_manager::get(theme_manager::BUTTON_HOVER);
        hist->press_color = theme_manager::get(theme_manager::BUTTON_PRESS);
        button* raw = hist.get();
        hist->on_click = [this, raw]() {
            point p = raw->to_screen(0.0f, raw->height);
            show_history(p.x, p.y);
        };
        history_btn_ = raw;
        tb->add_header_action(std::move(hist));
        tb->set_new_tab_callback([this]() { open_new_chat(); });
        tb->set_on_tab_context_menu([this](int index, float x, float y) {
            show_tab_menu(index, x, y);
        });
    }

    api->log_info(fmt::format("agent extension initialized ({} model(s))", models_.size()));
    return true;
}

void extension::shutdown() {
    if (api) api->log_info("agent extension shutdown");

    if (models_subscription_ != 0) {
        agent_registry::instance().off_models_changed(models_subscription_);
        models_subscription_ = 0;
    }
    if (auto* root = current_root()) {
        root->dismiss_context_menu();
        root->dismiss_popup();
        root->remove_menu_items(id());
    }
    if (auto* tb = current_tab_bar()) {
        tb->set_new_tab_callback(nullptr);
        tb->set_on_tab_context_menu(nullptr);
        tb->clear_header_actions();
    }
    history_btn_ = nullptr;

    // 标签页仍由标签栏持有：先摘掉所有指向本拓展的回调，再放弃引用。
    for (agent_tab* tab : tabs_) {
        if (!tab) continue;
        tab->set_repaint_callback(nullptr);
        tab->on_config_changed = nullptr;
        tab->on_delete_requested = nullptr;
        tab->on_destroyed = nullptr;
    }
    tabs_.clear();

#if !defined(__OHOS__)
    terminal_manager::instance().close_all();
#endif
    store_.reset();
}

std::vector<std::unique_ptr<tool>> extension::build_tools(const std::string& uuid) const {
    std::vector<std::unique_ptr<tool>> tools;
    auto workdir = [store = store_, uuid]() -> ghc::filesystem::path {
        return store ? store->conversation_dir(uuid) : ghc::filesystem::path();
    };
    auto scope = [uuid]() { return uuid; };

#if !defined(__OHOS__)
    auto create_terminal = std::make_unique<create_terminal_tool>();
    create_terminal->set_workdir(workdir);
    create_terminal->set_scope(scope);
    auto write_terminal = std::make_unique<write_terminal_tool>();
    write_terminal->set_scope(scope);
    auto read_terminal = std::make_unique<read_terminal_tool>();
    read_terminal->set_scope(scope);
    auto kill_terminal = std::make_unique<kill_terminal_tool>();
    kill_terminal->set_scope(scope);
    tools.push_back(std::move(create_terminal));
    tools.push_back(std::move(write_terminal));
    tools.push_back(std::move(read_terminal));
    tools.push_back(std::move(kill_terminal));
#endif

    auto create_file = std::make_unique<create_file_tool>();
    create_file->set_workdir(workdir);
    auto create_directory = std::make_unique<create_directory_tool>();
    create_directory->set_workdir(workdir);
    auto read_file = std::make_unique<read_file_tool>();
    read_file->set_workdir(workdir);
    auto edit_file = std::make_unique<edit_file_tool>();
    edit_file->set_workdir(workdir);
    auto rename = std::make_unique<rename_tool>();
    rename->set_workdir(workdir);
    auto remove = std::make_unique<delete_tool>();
    remove->set_workdir(workdir);
    tools.push_back(std::move(create_file));
    tools.push_back(std::move(create_directory));
    tools.push_back(std::move(read_file));
    tools.push_back(std::move(edit_file));
    tools.push_back(std::move(rename));
    tools.push_back(std::move(remove));

    tools.push_back(std::make_unique<fetch_tool>());
    tools.push_back(std::make_unique<sleep_tool>());

    auto todo = std::make_unique<todo_tool>();
    todo->bind_uuid(scope);
    tools.push_back(std::move(todo));

    auto memory = std::make_unique<memory_tool>();
    memory->bind(store_.get(), scope);
    tools.push_back(std::move(memory));

    return tools;
}

std::shared_ptr<chat_client> extension::make_client(
    const std::string& uuid, std::vector<std::unique_ptr<tool>>& tools) const {
    auto client = std::make_shared<chat_client>(default_cfg_);
    client->set_system_prompt(fmt::format(R"(
You're a Spiration agent in {}, with set of tools and strong ability.

Your tool call will be defaultly called in this chat workspace, where feel free to store any thing.

Think first, and do fast.
)", platform::os_name()));

    tools = build_tools(uuid);
    for (auto& t : tools) {
        if (t) client->register_tool(t.get());
    }
    for (auto* t : agent_registry::instance().tools()) {
        if (t) client->register_tool(t);
    }
    return client;
}

void extension::apply_models(agent_tab& tab) {
    auto merged = models_;
    merge_external_models(merged);
    tab.set_models(std::move(merged));
}

void extension::open_tab_for(const std::string& uuid, const std::string& name) {
    if (!api || !store_ || uuid.empty()) return;

    if (auto* tb = current_tab_bar()) {
        for (int i = 0; i < tb->tab_count(); ++i) {
            if (auto* existing = dynamic_cast<agent_tab*>(tb->get_tab(i))) {
                if (existing->conversation_uuid() == uuid) {
                    tb->activate_tab(i);
                    return;
                }
            }
        }
    }

    std::shared_ptr<chat_client> client;
    std::vector<std::unique_ptr<tool>> tools;
    if (!models_.empty()) client = make_client(uuid, tools);

    auto tab = std::make_unique<agent_tab>(client, store_, uuid, name);
    agent_tab* raw = tab.get();
    tab->set_tools(std::move(tools));
    tab->set_repaint_callback([this]() { if (api) api->request_repaint(); });
    tab->on_config_changed = [this](const chat_client::config& c) { default_cfg_ = c; };
    tab->on_destroyed = [this, raw]() { forget_tab(raw); };
    tab->on_delete_requested = [this](agent_tab* t) { close_tab_for(t); };
    apply_models(*tab);
    tabs_.push_back(raw);
    api->open_tab(std::move(tab));
}

void extension::open_new_chat() {
    if (!store_) return;
    std::string uuid = store_->create("");
    if (uuid.empty()) {
        if (api) api->log_warning("create conversation failed");
        return;
    }
    open_tab_for(uuid, "");
}

void extension::open_conversation(const std::string& uuid, const std::string& name) {
    open_tab_for(uuid, name);
}

void extension::forget_tab(agent_tab* tab) {
    tabs_.erase(std::remove(tabs_.begin(), tabs_.end(), tab), tabs_.end());
}

agent_tab* extension::find_tab_by_uuid(const std::string& uuid) const {
    auto* tb = current_tab_bar();
    if (!tb || uuid.empty()) return nullptr;
    for (int i = 0; i < tb->tab_count(); ++i) {
        auto* t = dynamic_cast<agent_tab*>(tb->get_tab(i));
        if (t && t->conversation_uuid() == uuid) return t;
    }
    return nullptr;
}

void extension::close_tab_for(agent_tab* tab) {
    if (!tab) return;
    auto* tb = current_tab_bar();
    if (!tb) return;
    for (int i = 0; i < tb->tab_count(); ++i) {
        if (tb->get_tab(i) == tab) {
            tb->close_tab(i);
            return;
        }
    }
}

void extension::show_history(float x, float y) {
    if (!history_btn_) return;
    auto all = store_ ? store_->list() : std::vector<conversation_meta>{};
    // 只列出有内容的对话，避免空会话堆积。
    std::vector<conversation_meta> convos;
    for (auto& meta : all) {
        if (meta.message_count > 0) convos.push_back(std::move(meta));
    }
    auto menu = std::make_unique<context_menu>();
    menu->add_item(i18n_manager::get().tr("agent:chat.new_chat"), [this]() { open_new_chat(); });
    if (!convos.empty()) menu->add_separator();
    const size_t limit = std::min<size_t>(convos.size(), 12);
    for (size_t i = 0; i < limit; ++i) {
        menu->add_item(convos[i].name,
                       [this, uuid = convos[i].uuid, name = convos[i].name]() {
                           open_conversation(uuid, name);
                       });
    }
    history_btn_->request_context_menu(x, y, std::move(menu));
}

void extension::show_tab_menu(int index, float x, float y) {
    auto* tb = current_tab_bar();
    if (!tb) return;
    tab* t = tb->get_tab(index);
    if (!t) return;

    auto menu = std::make_unique<context_menu>();
    if (auto* at = dynamic_cast<agent_tab*>(t)) {
        tb->activate_tab(index);
        const std::string uuid = at->conversation_uuid();
        menu->add_item(i18n_manager::get().tr("agent:chat.rename"), [this, uuid, tb]() {
            auto* target = find_tab_by_uuid(uuid);
            if (!target) return;
            for (int i = 0; i < tb->tab_count(); ++i) {
                if (tb->get_tab(i) == target) {
                    tb->activate_tab(i);
                    break;
                }
            }
            target->begin_rename();
        });
        menu->add_item(i18n_manager::get().tr("agent:chat.delete"), [this, uuid]() {
            if (auto* target = find_tab_by_uuid(uuid)) target->begin_delete();
        });
        menu->add_separator();
    }
    menu->add_item(i18n_manager::get().tr("agent:chat.close_tab"),
                   [tb, index, t]() {
                       // 指针只用于同一性比较，不 deref，标签已关闭时安全跳过。
                       if (tb->get_tab(index) == t) tb->close_tab(index);
                   });
    t->request_context_menu(x, y, std::move(menu));
}

void extension::on_models_changed() {
    for (agent_tab* tab : tabs_) {
        if (tab) apply_models(*tab);
    }
}

void extension::merge_external_models(std::vector<model_option>& models) const {
    for (auto& m : agent_registry::instance().collect_models()) {
        bool dup = false;
        for (const auto& mm : models) {
            if (mm.cfg.model == m.cfg.model) { dup = true; break; }
        }
        if (dup) continue;
        models.push_back(std::move(m));
    }
}

void extension::configure_models() {
    if (!api) return;
    ghc::filesystem::path models_path = data_dir_ / "cnf/models.json";
    if (!ghc::filesystem::exists(models_path)) {
        api->log_warning(fmt::format("models.json not found: {}", models_path.string()));
    }

    edit::edit_service* svc = api->get_service<edit::edit_service>(
        edit::edit_service::EXTENSION_ID, edit::edit_service::SERVICE_NAME);
    if (!svc) {
        api->log_warning("edit service unavailable");
        return;
    }
    svc->open_file(models_path);
}

} // namespace agent
} // namespace spiration
