/**
 * @file edit_tab.h
 * @brief 编辑标签页。
 * @author clk
 */

#pragma once

#include <ui/tab_bar.h>
#include <ui/text_area.h>
#include <functional>
#include <string>

namespace spiration {
namespace edit {

/**
 * @brief 文本编辑标签页。
 */
class edit_tab : public tab {
public:
    /** @brief 构造空编辑标签页。 */
    edit_tab();

    /**
     * @brief 构造指定标题的编辑标签页。
     * @param title 初始标题
     */
    explicit edit_tab(const std::string& title);

    /** @brief 绘制标签页内容。 */
    void paint(std::shared_ptr<renderer> renderer) override;

    /** @brief 布局：编辑器撑满整个标签页。 */
    void layout() override;

    /** @brief 标签页被激活时调用。 */
    void on_activate() override { if (activate_cb_) activate_cb_(); }

    /** @brief 标签页被取消激活时调用。 */
    void on_deactivate() override { if (deactivate_cb_) deactivate_cb_(); }

    /** @brief 设置编辑器的文本内容（重置光标/滚动）。 */
    void set_text(const std::string& text);

    /** @brief 从文件加载文本内容。 */
    void load_file(const std::string& path);

    /**
     * @brief 以预读内容打开。
     * @param path    文件路径/URI
     * @param content 已读取的文件内容
     */
    void open_content(const std::string& path, const std::string& content);

    /** @brief 保存当前内容到已打开的文件。 */
    bool save();

    /** @brief 另存当前内容到指定路径。 */
    bool save_as(const std::string& path);

    /**
     * @brief 确认已保存到指定路径。
     * @param path 目标路径/URI
     */
    void confirm_saved(const std::string& path);

    /** @brief 获取编辑器的文本内容。 */
    const std::string& text() const { return editor_->text; }

    /** @brief 获取当前关联的文件路径。 */
    const std::string& file_path() const { return file_path_; }

    /** @brief 获取光标行号。 */
    size_t cursor_line() const { return editor_->cursor_line(); }

    /** @brief 获取光标列号。 */
    size_t cursor_col() const { return editor_->cursor_col(); }

    /** @brief 设置保存回调（Ctrl+S 触发）。 */
    void set_save_callback(std::function<void()> cb) { save_cb_ = std::move(cb); }

    /** @brief 设置激活回调。 */
    void set_activate_callback(std::function<void()> cb) { activate_cb_ = std::move(cb); }

    /** @brief 设置取消激活回调。 */
    void set_deactivate_callback(std::function<void()> cb) { deactivate_cb_ = std::move(cb); }

    /** @brief 获取内嵌编辑器控件（供宿主直接操作）。 */
    text_area* editor() const { return editor_; }

private:
    /** @brief 内嵌编辑器子控件。 */
    text_area* editor_ = nullptr;
    /** @brief 当前关联的文件路径，空串表示未保存。 */
    std::string file_path_;
    /** @brief 内容是否已修改未保存。 */
    bool dirty_ = false;
    /** @brief 不含脏标记的基础标题。 */
    std::string base_title_;
    /** @brief 保存回调。 */
    std::function<void()> save_cb_;
    /** @brief 标签页激活回调。 */
    std::function<void()> activate_cb_;
    /** @brief 标签页取消激活回调。 */
    std::function<void()> deactivate_cb_;

    /** @brief 标记内容已修改，更新标题指示符。 */
    void mark_dirty();
    /** @brief 标记内容已保存，更新标题指示符。 */
    void mark_clean();
    /** @brief 根据 dirty_ 状态更新标签标题。 */
    void update_title();

    /** @brief 允许打开的最大文件大小。 */
    static constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;
};

} // namespace edit
} // namespace spiration