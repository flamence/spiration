/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <application.h>
#include <extension/builtin/edit/edit_tab.h>
#include <extension/builtin/i18n/i18n.h>
#include <extension/extension.h>
#include <string>

namespace spiration {
namespace edit {

/**
 * @brief 内置拓展编辑。
 */
class extension : public spiration::extension {
public:
    std::string id() const override          { return ID; }
    std::string name() const override        { return api->tr("extension.edit.name"); }
    std::string version() const override     { return "0.4"; }
    std::string description() const override { return api->tr("extension.edit.description"); }

    bool initialize() override;
    void shutdown() override;

    static inline std::string ID = "com.flamence.spiration.edit";

private:
    /** @brief 所有打开的编辑器标签页指针列表。 */
    std::vector<edit_tab*> editors_;

    /** @brief 获取当前激活的编辑器标签页。 */
    edit_tab* active_editor();
    /**
     * @brief 为编辑器标签页设置回调。
     * @param tab 目标编辑器标签页
     */
    void setup_editor_callbacks(edit_tab* tab);
    /** @brief 创建并打开一个新的空白编辑标签页。 */
    void new_editor_tab();
    /** @brief 弹出文件选择对话框，打开文件到新标签页。 */
    void open_editor_tab();
    /**
     * @brief 文件选择完成后按路径打开。
     * @param path    路径/URI
     * @param content 预读内容
     */
    void complete_open_editor_tab(const std::string& path, const std::string& content = "");
    /** @brief 保存当前激活的编辑标签页。 */
    void save_current();
    /** @brief 另存当前激活的编辑标签页到新路径。 */
    void save_current_as();
    /** @brief 另存对话框完成后按路径保存。 */
    void complete_save_as(const std::string& path);
};

} // namespace edit
} // namespace spiration
