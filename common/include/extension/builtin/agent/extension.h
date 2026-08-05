/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/chat.h>
#include <extension/builtin/agent/chat_store.h>
#include <extension/builtin/agent/registry.h>
#include <extension/builtin/agent/agent_tab.h>
#include <extension/builtin/agent/tool/terminal_tool.h>
#include <extension/builtin/agent/tool/edit_tool.h>
#include <extension/builtin/agent/tool/web_tool.h>
#include <extension/builtin/agent/tool/todo_tool.h>
#include <extension/builtin/agent/tool/sleep_tool.h>
#include <extension/builtin/agent/tool/memory_tool.h>
#include <extension/builtin/i18n/i18n.h>
#include <extension/extension.h>

#include <memory>

namespace spiration {
namespace agent {

class extension : public spiration::extension {
public:
    std::string id() const override          { return ID; }
    std::string name() const override        { return api->tr("extension.agent.name"); }
    std::string version() const override     { return "0.7"; }
    std::string description() const override { return api->tr("extension.agent.description"); }

    bool initialize() override;
    void shutdown() override;

    static inline std::string ID = "com.flamence.spiration.agent";

private:
    void open_agent_tab();
    /// @brief 将当前对话保存到当前会话的 UUID 目录。
    void save_conversation(agent_tab* tab);

    std::unique_ptr<chat_client> client_;
    std::unique_ptr<chat_store> store_;
    std::string data_dir_;
    /// 模型选项。
    std::vector<model_option> models_;
    /// 当前打开的智能体标签页。
    agent_tab* agent_tab_ = nullptr;

    std::unique_ptr<create_terminal_tool> create_terminal_;
    std::unique_ptr<write_terminal_tool> write_terminal_;
    std::unique_ptr<read_terminal_tool> read_terminal_;
    std::unique_ptr<kill_terminal_tool> kill_terminal_;
    std::unique_ptr<create_file_tool> create_file_;
    std::unique_ptr<read_file_tool> read_file_;
    std::unique_ptr<create_directory_tool> create_directory_;
    std::unique_ptr<edit_file_tool> edit_file_;
    std::unique_ptr<rename_tool> rename_;
    std::unique_ptr<delete_tool> delete_;
    std::unique_ptr<fetch_tool> fetch_;
    std::unique_ptr<todo_tool> todo_;
    std::unique_ptr<sleep_tool> sleep_;
    std::unique_ptr<memory_tool> memory_;
};

} // namespace agent
} // namespace spiration
