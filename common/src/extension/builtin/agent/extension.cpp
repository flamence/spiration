/**
 * @file extension.cpp
 * @brief 智能体拓展实现。
 * @author clk
 */

#include <extension/builtin/agent/extension.h>
#include <extension/builtin/i18n/i18n.h>
#include <extension/extension_api.h>
#include <extension/extension_loader.h>
#include <extension/extension_manager.h>
#include <utils/console.h>
#include <utils/platform.h>

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
    std::string rs = m.value("reasoning", "standard");
    if (rs == "none")       opt.cfg.reasoning = reasoning_level::none;
    else if (rs == "deep")  opt.cfg.reasoning = reasoning_level::deep;
    else                    opt.cfg.reasoning = reasoning_level::standard;
    return opt;
}

} // namespace

bool extension::initialize() {
    if (!api) return false;

    data_dir_ = api->extension_data_dir(id());
    std::string models_path = data_dir_ + "/cnf/models.json";
    std::string legacy_path = data_dir_ + "/cnf/model.json";

    chat_client::config cfg;  // 默认/回退配置
    bool has_config = false;

    // 首选 cnf/models.json：对象（按模型 id 键）或数组（每项完整配置）
    if (platform::file_exists(models_path)) {
        std::string json = extension_loader::read_file_text(models_path);
        if (!json.empty()) {
            try {
                nlohmann::json j = nlohmann::json::parse(json);
                if (j.is_object()) {
                    for (auto& [key, m] : j.items()) {
                        if (m.is_object()) models_.push_back(parse_model_json(m, key));
                    }
                } else if (j.is_array()) {
                    for (auto& m : j) {
                        if (m.is_object()) models_.push_back(parse_model_json(m, ""));
                    }
                }
            } catch (const std::exception& e) {
                api->log_error("parse cnf/models.json failed: %s", e.what());
            }
        }
        has_config = !models_.empty();
        if (has_config) cfg = models_.front().cfg;
    } else if (platform::file_exists(legacy_path)) {
        // 旧版单配置 model.json（全局配置 + 可选 models[]）
        std::string json = extension_loader::read_file_text(legacy_path);
        if (!json.empty()) {
            try {
                nlohmann::json j = nlohmann::json::parse(json);
                cfg.endpoint    = j.value("endpoint", cfg.endpoint);
                cfg.api_key     = j.value("api_key", cfg.api_key);
                cfg.model       = j.value("model", cfg.model);
                cfg.provider    = j.value("provider", cfg.provider);
                cfg.max_tokens  = j.value("max_tokens", cfg.max_tokens);
                cfg.temperature = j.value("temperature", cfg.temperature);
                cfg.stream      = j.value("stream", cfg.stream);
                cfg.timeout_seconds = j.value("timeout", cfg.timeout_seconds);
                std::string rs  = j.value("reasoning", "standard");
                if (rs == "none")       cfg.reasoning = reasoning_level::none;
                else if (rs == "deep")  cfg.reasoning = reasoning_level::deep;
                else                    cfg.reasoning = reasoning_level::standard;
                if (j.contains("models") && j["models"].is_array()) {
                    for (auto& m : j["models"]) {
                        if (m.is_string()) {
                            model_option opt;
                            opt.display_name = m.get<std::string>();
                            opt.cfg = cfg;
                            opt.cfg.model = m.get<std::string>();
                            models_.push_back(std::move(opt));
                        } else if (m.is_object()) {
                            model_option opt = parse_model_json(m, "");
                            if (opt.cfg.endpoint == chat_client::config().endpoint)
                                opt.cfg.endpoint = cfg.endpoint;
                            if (opt.cfg.api_key.empty()) opt.cfg.api_key = cfg.api_key;
                            if (opt.cfg.provider == "openai") opt.cfg.provider = cfg.provider;
                            models_.push_back(std::move(opt));
                        }
                    }
                }
            } catch (const std::exception& e) {
                api->log_error("parse cnf/model.json failed: %s", e.what());
            }
        }
        has_config = !cfg.model.empty() || !models_.empty();
    }

    if (has_config) {
        client_ = std::make_unique<chat_client>(cfg);
        client_->set_system_prompt(
            "你是一个有用的助手，可使用以下工具集："
            "1) 终端：create_terminal 创建交互式终端会话得到 terminal_id，write_terminal 发送命令（默认附带回车），read_terminal 按行窗口读取输出（行号自下而上，1 为最新行），会话保持存活可连续操作，kill_terminal 关闭并释放终端；"
            "2) 文件编辑：create_file 创建文件，create_directory 创建目录，read_file 读取文件（可指定 start_line/end_line 行范围），edit_file 编辑文件（content 全量重写或 search+replace 查找替换），rename 重命名/移动，delete 删除文件或目录；文件操作请优先使用相对路径（默认保存到当前会话目录），读取/编辑程序自身文件时才用绝对路径；"
            "3) Web：fetch 拉取网址内容（支持方法/请求头/查询参数/请求体/超时）；"
            "4) 规划：todo 维护可追踪的任务列表（status 取 pending/in_progress/completed，每次传全量列表）；"
            "5) 等待：sleep 休眠指定秒数，用于等待编译等后台任务完成后再读取输出；"
            "6) 记忆：memory 读写当前会话的持久记忆（action 取 read/write/append/clear），重要上下文可存入。"
            "注意：write_terminal 发送命令前需要用户确认，被拒绝时结果会提示 user denied。"
            "回复请用中文。");

        create_terminal_ = std::make_unique<create_terminal_tool>();
        write_terminal_ = std::make_unique<write_terminal_tool>();
        read_terminal_ = std::make_unique<read_terminal_tool>();
        kill_terminal_ = std::make_unique<kill_terminal_tool>();
        client_->register_tool(create_terminal_.get());
        client_->register_tool(write_terminal_.get());
        client_->register_tool(read_terminal_.get());
        client_->register_tool(kill_terminal_.get());

        create_file_ = std::make_unique<create_file_tool>();
        create_directory_ = std::make_unique<create_directory_tool>();
        read_file_ = std::make_unique<read_file_tool>();
        edit_file_ = std::make_unique<edit_file_tool>();
        rename_ = std::make_unique<rename_tool>();
        delete_ = std::make_unique<delete_tool>();
        // 文件工具相对路径默认解析到当前会话的 chat/<uuid>/ 目录，
        // 防止 agent 在程序运行路径下生成大量脚本/文件
        auto workdir = [this]() -> std::string {
            return store_ ? store_->conversation_dir(store_->current_uuid()) : std::string();
        };
        create_file_->set_workdir(workdir);
        create_directory_->set_workdir(workdir);
        read_file_->set_workdir(workdir);
        edit_file_->set_workdir(workdir);
        rename_->set_workdir(workdir);
        delete_->set_workdir(workdir);
        // 终端初始工作目录同样默认 = 会话目录
        create_terminal_->set_workdir(workdir);
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

        // 记忆工具（读写当前会话 memory.md）
        memory_ = std::make_unique<memory_tool>();
        client_->register_tool(memory_.get());

        api->log_info("agent extension initialized (provider=%s, endpoint=%s, model=%s, %zu model(s))",
                      client_->provider_name().c_str(), cfg.endpoint.c_str(),
                      cfg.model.c_str(), models_.size());
    } else {
        api->log_info("agent extension initialized (no config at %s)", models_path.c_str());
    }

    // 聊天记录存档目录（与 cnf/ 同级）
    store_ = std::make_unique<chat_store>(data_dir_);

    if (memory_) {
        memory_->bind(store_.get(), [this]() { return store_->current_uuid(); });
    }

    // 纳入其它拓展经注册表注册的工具
    if (client_) {
        for (auto* t : agent_registry::instance().tools()) {
            if (t) client_->register_tool(t);
        }
    }

    // 注册服务，供其它拓展查询（注册 provider / tool）
    extension_manager::register_service(id(), agent_registry::SERVICE_NAME,
                                        &agent_registry::instance());

    api->add_menu_item("menu.help", i18n_manager::get().tr("menu.help.agent"), [this]() {
        open_agent_tab();
    });

    return true;
}

void extension::shutdown() {
    if (api) api->log_info("agent extension shutdown");
    save_conversation(nullptr);
    if (agent_tab_) {
        agent_tab_->on_conversation_done = nullptr;
        agent_tab_->on_destroyed = nullptr;
        agent_tab_ = nullptr;
    }
    terminal_manager::instance().close_all();
    if (client_) client_->wait_all_tools();
    create_terminal_.reset();
    write_terminal_.reset();
    read_terminal_.reset();
    kill_terminal_.reset();
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
    // 只允许一个智能体标签页：已存在则激活，否则新建
    if (agent_tab_) {
        if (api->activate_tab(agent_tab_)) return;
        agent_tab_ = nullptr;  // 标签页已关闭（指针悬空，仅作比较用）
    }
    auto tab = std::make_unique<agent_tab>(client_.get(), store_.get());
    tab->set_repaint_callback([this]() { if (api) api->request_repaint(); });
    tab->on_conversation_done = [this](agent_tab* t) { save_conversation(t); };
    tab->on_destroyed = [this]() { agent_tab_ = nullptr; };  // 标签页关闭时清引用
    tab->set_models(models_);
    agent_tab_ = tab.get();
    api->open_tab(std::move(tab));
}

void extension::save_conversation(agent_tab* tab) {
    if (!client_ || !store_) return;
    std::string uuid = store_->current_uuid();
    if (uuid.empty()) return;

    chat_archive archive;
    chat_archive existing;
    if (store_->load(uuid, existing)) {
        archive.name       = existing.name;
        archive.created_at = existing.created_at;
        archive.memory     = existing.memory;
    }
    archive.uuid      = uuid;
    archive.provider  = client_->provider_name();
    archive.model     = client_->get_config().model;
    archive.messages  = client_->history();
    archive.terminals = terminal_manager::instance().snapshots();
    archive.todos     = todo_store::instance().items();
    archive.tokens_in  = client_->input_tokens();
    archive.tokens_out = client_->output_tokens();
    if (tab) {
        archive.can_continue = tab->can_continue();
        archive.auto_approve = tab->auto_approve();
    }
    store_->save(archive);
}

} // namespace agent
} // namespace spiration
