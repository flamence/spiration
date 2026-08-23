/**
 * @file extension.cpp
 * @brief 智能体拓展实现。
 * @author clk
 */

#include <fmt/core.h>
#include <spiration/extension/builtin/agent/extension.h>
#include <spiration/extension/builtin/edit/edit_service.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/extension/extension_api.h>
#include <spiration/extension/extension_manager.h>
#include <spiration/utils/dispatcher.h>
#include <ghc/filesystem.hpp>
#include <fmt/format.h>
#include <spiration/utils/platform.h>
#include <spiration/utils/io.h>

#include <nlohmann/json.hpp>

namespace spiration {
namespace agent {

namespace {

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

    chat_client::config cfg;
    bool has_config = false;

    if (ghc::filesystem::exists(models_path)) {
        std::string json = io::read_file_content(models_path);
        if (!json.empty()) {
            try {
                nlohmann::json j = nlohmann::json::parse(json);
                if (j.is_object()) {
                    for (auto& item : j.items()) {
                        const auto& key = item.key();
                        auto& m = item.value();
                        if (m.is_object()) models_.push_back(parse_model_json(m, key));
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
        has_config = !models_.empty();
        if (has_config) cfg = models_.front().cfg;
    }

    if (has_config) {
        client_ = std::make_shared<chat_client>(cfg);
        client_->set_system_prompt(fmt::format(R"(
You're a Spiration agent in {}, with set of tools and strong ability.

Your tool call will be defaultly called in this chat workspace, where feel free to store any thing.

Think first, and do fast.
)", platform::os_name()));

#if !defined(__OHOS__)
        create_terminal_ = std::make_unique<create_terminal_tool>();
        write_terminal_ = std::make_unique<write_terminal_tool>();
        read_terminal_ = std::make_unique<read_terminal_tool>();
        kill_terminal_ = std::make_unique<kill_terminal_tool>();
        client_->register_tool(create_terminal_.get());
        client_->register_tool(write_terminal_.get());
        client_->register_tool(read_terminal_.get());
        client_->register_tool(kill_terminal_.get());
#endif

        create_file_ = std::make_unique<create_file_tool>();
        create_directory_ = std::make_unique<create_directory_tool>();
        read_file_ = std::make_unique<read_file_tool>();
        edit_file_ = std::make_unique<edit_file_tool>();
        rename_ = std::make_unique<rename_tool>();
        delete_ = std::make_unique<delete_tool>();
        auto workdir = [this]() -> ghc::filesystem::path {
            return store_ ? store_->conversation_dir(store_->current_uuid()) : ghc::filesystem::path();
        };
        create_file_->set_workdir(workdir);
        create_directory_->set_workdir(workdir);
        read_file_->set_workdir(workdir);
        edit_file_->set_workdir(workdir);
        rename_->set_workdir(workdir);
        delete_->set_workdir(workdir);
#if !defined(__OHOS__)
        create_terminal_->set_workdir(workdir);
#endif
        client_->register_tool(create_file_.get());
        client_->register_tool(create_directory_.get());
        client_->register_tool(read_file_.get());
        client_->register_tool(edit_file_.get());
        client_->register_tool(rename_.get());
        client_->register_tool(delete_.get());

        fetch_ = std::make_unique<fetch_tool>();
        client_->register_tool(fetch_.get());

        todo_ = std::make_unique<todo_tool>();
        client_->register_tool(todo_.get());

        sleep_ = std::make_unique<sleep_tool>();
        client_->register_tool(sleep_.get());

        memory_ = std::make_unique<memory_tool>();
        client_->register_tool(memory_.get());

        api->log_info(fmt::format("agent extension initialized ({} model(s))", models_.size()));
    } else {
        api->log_info(fmt::format("agent extension initialized (no config at {})", models_path.string()));
    }

    store_ = std::make_shared<chat_store>(data_dir_);

    if (memory_) {
        memory_->bind(store_.get(), [this]() { return store_->current_uuid(); });
    }

    if (client_) {
        for (auto* t : agent_registry::instance().tools()) {
            if (t) client_->register_tool(t);
        }
    }

    application::instance()->extension()->register_service(id(), agent_registry::SERVICE_NAME,
                                        &agent_registry::instance());

    api->add_menu_item("menu.help", i18n_manager::get().tr("agent:menu.help.agent"), [this]() {
        open_agent_tab();
    });

    api->add_menu_item("menu.help", i18n_manager::get().tr("agent:menu.help.configure_models"), [this]() {
        configure_models();
    });

    // 订阅模型来源变更（外部拓展增减模型后刷新已打开的智能体标签页下拉框）。
    agent_registry::instance().on_models_changed([this]() { on_models_changed(); });

    return true;
}

void extension::shutdown() {
    shutting_down_ = true;
    if (api) api->log_info("agent extension shutdown");
    save_conversation(nullptr);
    if (agent_tab_) {
        agent_tab_->on_conversation_done = nullptr;
        agent_tab_->on_destroyed = nullptr;
        agent_tab_->wait_initial_load();
        agent_tab_ = nullptr;
    }
#if !defined(__OHOS__)
    terminal_manager::instance().close_all();
#endif
    if (client_) client_->wait_all_tools();
#if !defined(__OHOS__)
    create_terminal_.reset();
    write_terminal_.reset();
    read_terminal_.reset();
    kill_terminal_.reset();
#endif
    create_file_.reset();
    create_directory_.reset();
    edit_file_.reset();
    rename_.reset();
    delete_.reset();
    fetch_.reset();
    todo_.reset();
    sleep_.reset();
    memory_.reset();
    client_.reset();
    store_.reset();
}

void extension::open_agent_tab() {
    if (agent_tab_) {
        if (api->activate_tab(agent_tab_)) {
            // 已打开的标签页也重新合并外部模型，保证下拉框最新。
            auto merged = models_;
            merge_external_models(merged);
            agent_tab_->set_models(std::move(merged));
            return;
        }
        agent_tab_ = nullptr;
    }
    auto tab = std::make_unique<agent_tab>(client_, store_);
    tab->set_repaint_callback([this]() { if (api) api->request_repaint(); });
    tab->on_conversation_done = [this](agent_tab* t) { save_conversation(t); };
    tab->on_destroyed = [this]() { agent_tab_ = nullptr; };  // 标签页关闭时清引用
    auto merged = models_;
    merge_external_models(merged);
    tab->set_models(std::move(merged));
    agent_tab_ = tab.get();
    api->open_tab(std::move(tab));
}

void extension::on_models_changed() {
    // 外部模型来源变更后刷新已打开标签页的下拉框（未打开则下次 open 时合并）。
    if (!agent_tab_) return;
    auto merged = models_;
    merge_external_models(merged);
    agent_tab_->set_models(std::move(merged));
}

void extension::merge_external_models(std::vector<model_option>& models) const {
    // 外部拓展通过注册表注册模型来源，向智能体提供可选模型；按 cfg.model 去重后并入下拉框。
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

void extension::save_conversation(agent_tab* tab) {
    if (!client_ || !store_) return;
    std::string uuid = store_->current_uuid();
    if (uuid.empty()) return;

    chat_archive archive;
    chat_archive existing;
    try {
        if (store_->load(uuid, existing)) {
            archive.name       = existing.name;
            archive.created_at = existing.created_at;
            archive.memory     = existing.memory;
        }
        archive.uuid      = uuid;
        archive.provider  = client_->provider_name();
        archive.model     = client_->get_config().model;
        archive.messages  = client_->history();
#if !defined(__OHOS__)
        archive.terminals = terminal_manager::instance().snapshots();
#endif
        archive.todos     = todo_store::instance().items();
        archive.tokens_in  = client_->input_tokens();
        archive.tokens_out = client_->output_tokens();
        if (tab) {
            archive.can_continue = tab->can_continue();
            archive.auto_approve = tab->auto_approve();
        }
    } catch (const std::exception& e) {
        if (api) api->log_error(fmt::format("save conversation failed: {}", e.what()));
        return;
    } catch (...) {
        if (api) api->log_error("save conversation failed: unknown exception");
        return;
    }
    if (auto* d = application::instance()->dispatcher()) {
        if (shutting_down_) {
            // 关闭中：dispatcher::shutdown 会清空任务队列，异步保存会被丢弃，
            // 必须同步写盘（这是“关闭应用后对话丢失”的根因）。
            store_->save(archive);
        } else {
            d->post([store = store_, archive = std::move(archive)]() {
                store->save(archive);
            });
        }
    } else {
        store_->save(archive);
    }
}

} // namespace agent
} // namespace spiration
