/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <spiration/extension/builtin/agent/agent_tab.h>
#include <spiration/extension/builtin/agent/chat.h>
#include <spiration/extension/builtin/agent/chat_store.h>
#include <spiration/extension/builtin/agent/registry.h>
#include <spiration/extension/builtin/agent/tool/tool.hpp>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/extension/extension.hpp>
#include <spiration/ui/button.h>

#include <ghc/filesystem.hpp>
#include <memory>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

/**
 * @brief 智能体拓展。
 *
 * 每个智能体标签页承载一个独立对话（独立 chat_client / 工具集 / 会话 UUID），
 * 拓展只负责创建、关闭与模型配置。
 */
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
    /// @brief 新建一个空白对话标签页。
    void open_new_chat();
    /// @brief 在标签页中打开已有对话；已打开则直接激活。
    void open_conversation(const std::string& uuid, const std::string& name);
    /// @brief 关闭承载指定标签页的标签栏项。
    void close_tab_for(agent_tab* tab);
    /// @brief 头部“历史”按钮：列出最近对话。
    void show_history(float x, float y);
    /// @brief 标签头右键菜单（重命名 / 删除 / 关闭）。
    void show_tab_menu(int index, float x, float y);
    /// @brief 在编辑器中打开 cnf/models.json 以配置模型。
    void configure_models();
    /// @brief 将外部模型来源（注册表）提供的模型合并进模型选项。
    void merge_external_models(std::vector<model_option>& models) const;
    /// @brief 模型来源变更回调：刷新已打开标签页的下拉框。
    void on_models_changed();
    /// @brief 为一个会话创建工具集（工具需比客户端存活更久）。
    std::vector<std::unique_ptr<tool>> build_tools(const std::string& uuid) const;
    /// @brief 为一个会话创建已注册工具的对话客户端。
    std::shared_ptr<chat_client> make_client(const std::string& uuid,
                                             std::vector<std::unique_ptr<tool>>& tools) const;
    /// @brief 把当前模型列表同步到标签页。
    void apply_models(agent_tab& tab);
    /// @brief 打开一个标签页（新对话或已有对话共用）。
    void open_tab_for(const std::string& uuid, const std::string& name);
    /// @brief 从 tabs_ 中移除已销毁的标签页指针。
    void forget_tab(agent_tab* tab);
    /// @brief 按会话 UUID 查找当前打开的标签页。
    agent_tab* find_tab_by_uuid(const std::string& uuid) const;

    std::shared_ptr<chat_store> store_;
    ghc::filesystem::path data_dir_;
    std::vector<model_option> models_;
    /// @brief 最近一次使用的模型配置，作为新建标签页的默认模型。
    chat_client::config default_cfg_;
    button* history_btn_ = nullptr;
    /// @brief 模型来源变更订阅 ID。
    int models_subscription_ = 0;
    /// @brief 当前打开的智能体标签页（顺序与标签栏无关，仅用于广播模型变更）。
    std::vector<agent_tab*> tabs_;
};

} // namespace agent
} // namespace spiration
