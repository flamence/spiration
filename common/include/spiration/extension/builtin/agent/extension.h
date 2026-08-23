/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <spiration/extension/builtin/agent/chat.h>
#include <spiration/extension/builtin/agent/chat_store.h>
#include <spiration/extension/builtin/agent/registry.h>
#include <spiration/extension/builtin/agent/agent_tab.h>
#include <spiration/extension/builtin/agent/tool/terminal_tool.h>
#include <spiration/extension/builtin/agent/tool/edit_tool.h>
#include <spiration/extension/builtin/agent/tool/web_tool.h>
#include <spiration/extension/builtin/agent/tool/todo_tool.h>
#include <spiration/extension/builtin/agent/tool/sleep_tool.h>
#include <spiration/extension/builtin/agent/tool/memory_tool.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/extension/extension.hpp>

#include <ghc/filesystem.hpp>
#include <memory>

namespace spiration {
namespace agent {

class extension : public spiration::extension {
public:
    extension() : spiration::extension(manifest_data({
        "com.flamence.spiration.agent",
        "0.7"
    })) {}

    tl::optional<std::string> name() const override { return api->tr("agent:name"); }
    tl::optional<std::string> description() const override { return api->tr("agent:description"); }
    bool initialize() override;
    void shutdown() override;

private:
    void open_agent_tab();
    /// @brief 将当前对话保存到当前会话的 UUID 目录。
    void save_conversation(agent_tab* tab);
    /// @brief 在编辑器中打开 cnf/models.json 以配置模型。
    void configure_models();
    /// @brief 将外部模型来源（注册表）提供的模型合并进模型选项。
    void merge_external_models(std::vector<model_option>& models) const;
    /// @brief 模型来源变更回调：刷新已打开的智能体标签页下拉框。
    void on_models_changed();

    std::shared_ptr<chat_client> client_;
    std::shared_ptr<chat_store> store_;
    ghc::filesystem::path data_dir_;
    /// 模型选项。
    std::vector<model_option> models_;
    /// 当前打开的智能体标签页。
    agent_tab* agent_tab_ = nullptr;
    /// 是否正在关闭（shutdown 时同步保存，避免任务被 dispatcher 清空丢弃）。
    bool shutting_down_ = false;

#if !defined(__OHOS__)
    std::unique_ptr<create_terminal_tool> create_terminal_;
    std::unique_ptr<write_terminal_tool> write_terminal_;
    std::unique_ptr<read_terminal_tool> read_terminal_;
    std::unique_ptr<kill_terminal_tool> kill_terminal_;
#endif
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
