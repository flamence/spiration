/**
 * @file extension.h
 * @brief 拓展入口。
 * @author clk
 */

#pragma once

#include <spiration/application.h>
#include <spiration/extension/builtin/edit/edit_tab.h>
#include <spiration/extension/builtin/edit/edit_service.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/extension/extension.hpp>
#include <string>

namespace spiration {
namespace edit {

/**
 * @brief 内置拓展编辑。
 *        同时实现 edit_service，供其它拓展将文件打开到编辑器。
 */
class extension : public spiration::extension, public edit_service {
public:
    extension() : spiration::extension({
        "com.flamence.spiration.edit",
        "0.4"
    }) {}

    tl::optional<std::string> name() const override { return api->tr("edit:name"); }
    tl::optional<std::string> description() const override { return api->tr("edit:description"); }

    bool initialize() override;
    void shutdown() override;

    void open_file(const ghc::filesystem::path& path) override;
    void open_file_with_content(const ghc::filesystem::path& path,
                                const std::string& content) override;

private:
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
