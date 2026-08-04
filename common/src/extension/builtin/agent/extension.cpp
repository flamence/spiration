/**
 * @file extension.cpp
 * @brief 智能体拓展实现。
 * @author clk
 */

#include <extension/builtin/agent/extension.h>
#include <extension/builtin/i18n/i18n.h>
#include <extension/extension_api.h>
#include <extension/extension_loader.h>
#include <utils/console.h>
#include <utils/platform.h>

#include <nlohmann/json.hpp>

namespace spiration {
namespace agent {

bool extension::initialize() {
    if (!api) return false;

    std::string config_path = api->extension_data_dir(id()) + "/cnf/model.json";
    bool has_config = platform::file_exists(config_path);

    if (has_config) {
        chat_client::config cfg;
        std::string json = extension_loader::read_file_text(config_path);
        if (!json.empty()) {
            nlohmann::json j = nlohmann::json::parse(json);
            cfg.endpoint    = j.value("endpoint", "");
            cfg.api_key     = j.value("api_key", "");
            cfg.model       = j.value("model", "");
            cfg.max_tokens  = j.value("max_tokens", cfg.max_tokens);
            cfg.temperature = j.value("temperature", cfg.temperature);
            cfg.stream      = j.value("stream", cfg.stream);
            std::string rs  = j.value("reasoning", "standard");
            if (rs == "none")       cfg.reasoning = reasoning_level::none;
            else if (rs == "deep")  cfg.reasoning = reasoning_level::deep;
            else                    cfg.reasoning = reasoning_level::standard;
        }

        client_ = std::make_unique<chat_client>(cfg);
        client_->set_system_prompt(
            "你是一个有用的助手，可使用以下工具集："
            "1) 终端：create_terminal 创建交互式终端会话得到 terminal_id，write_terminal 发送命令（默认附带回车），read_terminal 按行窗口读取输出（行号自下而上，1 为最新行），会话保持存活可连续操作；"
            "2) 文件编辑：create_file 创建文件，create_directory 创建目录，read_file 读取文件（可指定 start_line/end_line 行范围），edit_file 编辑文件（content 全量重写或 search+replace 查找替换），rename 重命名/移动，delete 删除文件或目录；"
            "3) Web：fetch 拉取网址内容（支持方法/请求头/查询参数/请求体/超时）。"
            "回复请用中文。");

        create_terminal_ = std::make_unique<create_terminal_tool>();
        write_terminal_ = std::make_unique<write_terminal_tool>();
        read_terminal_ = std::make_unique<read_terminal_tool>();
        client_->register_tool(create_terminal_.get());
        client_->register_tool(write_terminal_.get());
        client_->register_tool(read_terminal_.get());

        create_file_ = std::make_unique<create_file_tool>();
        create_directory_ = std::make_unique<create_directory_tool>();
        read_file_ = std::make_unique<read_file_tool>();
        edit_file_ = std::make_unique<edit_file_tool>();
        rename_ = std::make_unique<rename_tool>();
        delete_ = std::make_unique<delete_tool>();
        client_->register_tool(create_file_.get());
        client_->register_tool(create_directory_.get());
        client_->register_tool(read_file_.get());
        client_->register_tool(edit_file_.get());
        client_->register_tool(rename_.get());
        client_->register_tool(delete_.get());

        fetch_ = std::make_unique<fetch_tool>();
        client_->register_tool(fetch_.get());

        api->log_info("agent extension initialized (endpoint=%s, model=%s)",
                      cfg.endpoint.c_str(), cfg.model.c_str());
    } else {
        api->log_info("agent extension initialized (no config at %s)", config_path.c_str());
    }

    api->add_menu_item("menu.help", i18n_manager::get().tr("menu.help.agent"), [this]() {
        open_agent_tab();
    });

    return true;
}

void extension::shutdown() {
    if (api) api->log_info("agent extension shutdown");
    terminal_manager::instance().close_all();
    create_terminal_.reset();
    write_terminal_.reset();
    read_terminal_.reset();
    create_file_.reset();
    create_directory_.reset();
    edit_file_.reset();
    rename_.reset();
    delete_.reset();
    fetch_.reset();
    client_.reset();
}

void extension::open_agent_tab() {
    auto tab = std::make_unique<agent_tab>(client_.get());
    tab->set_repaint_callback([this]() { if (api) api->request_repaint(); });
    api->open_tab(std::move(tab));
}

} // namespace agent
} // namespace spiration
