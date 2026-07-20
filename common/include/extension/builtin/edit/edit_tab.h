/**
 * @file edit_tab.h
 * @brief 编辑标签页。
 * @author clk
 */

#pragma once

#include <ui/tab_bar.h>
#include <ui/color.h>
#include <utils/animation.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace spiration {
namespace edit {

/**
 * @brief 文本编辑标签页，提供多行文本编辑、光标导航、选择、滚动条等功能。
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

    /**
     * @brief 绘制编辑器内容。
     * @param renderer 渲染器实例
     */
    void paint(std::shared_ptr<renderer> renderer) override;

    /**
     * @brief 处理输入事件（键盘、鼠标等）。
     * @param type 事件类型
     * @param data 事件数据指针
     */
    void handle_event(const event_type& type, void* data) override;

    /**
     * @brief 逐帧更新动画状态（光标闪烁、颜色过渡等）。
     * @param dt_ms 距离上一帧的毫秒数
     */
    void tick(float dt_ms) override;

    /**
     * @brief 设置编辑器的文本内容。
     * @param text 新文本
     */
    void set_text(const std::string& text);

    /**
     * @brief 从文件加载文本内容。
     * @param path 文件路径
     */
    void load_file(const std::string& path);

    /**
     * @brief 保存当前内容到已打开的文件。
     * @return true 保存成功
     */
    bool save();

    /**
     * @brief 另存当前内容到指定路径。
     * @param path 目标文件路径
     * @return true 保存成功
     */
    bool save_as(const std::string& path);

    /** @brief 获取编辑器的文本内容。 */
    const std::string& text() const { return buffer_; }

    /** @brief 获取当前关联的文件路径。 */
    const std::string& file_path() const { return file_path_; }

    /** @brief 获取光标行号。 */
    size_t cursor_line() const { return cursor_line_; }

    /** @brief 获取光标列号。 */
    size_t cursor_col() const { return cursor_col_; }

    /**
     * @brief 设置重绘回调。
     * @param cb 回调函数
     */
    void set_repaint_callback(std::function<void()> cb) { repaint_cb_ = std::move(cb); }

    /**
     * @brief 设置保存回调。
     * @param cb 回调函数
     */
    void set_save_callback(std::function<void()> cb) { save_cb_ = std::move(cb); }

    /**
     * @brief 设置激活回调。
     * @param cb 回调函数
     */
    void set_activate_callback(std::function<void()> cb) { activate_cb_ = std::move(cb); }

    /** @brief 标签页被激活时调用。 */
    void on_activate() override { if (activate_cb_) activate_cb_(); }

    /** @brief 标签页被取消激活时调用。 */
    void on_deactivate() override { if (deactivate_cb_) deactivate_cb_(); }

    /**
     * @brief 设置取消激活回调。
     * @param cb 回调函数
     */
    void set_deactivate_callback(std::function<void()> cb) { deactivate_cb_ = std::move(cb); }

private:
    /** @brief 编辑器文本缓冲区。 */
    std::string buffer_;
    /** @brief 当前关联的文件路径，空串表示未保存。 */
    std::string file_path_;
    /** @brief 内容是否已修改未保存。 */
    bool dirty_ = false;

    /** @brief 每行起始索引缓存，加速行定位。 */
    std::vector<size_t> line_starts_;
    /** @brief line_starts_ 是否需要重建。 */
    bool line_starts_dirty_ = false;

    /** @brief 重建 line_starts_ 索引。 */
    void rebuild_line_starts();
    /** @brief 获取总行数。 */
    size_t line_count() const;
    /** @brief 获取第 n 行文本。 */
    std::string_view line_text(size_t n) const;

    /** @brief 光标行号。 */
    size_t cursor_line_ = 0;
    /** @brief 光标列号。 */
    size_t cursor_col_ = 0;
    /** @brief 垂直滚动偏移量。 */
    float scroll_y_ = 0.0f;
    /** @brief 水平滚动偏移量。 */
    float scroll_x_ = 0.0f;
    /** @brief 光标是否可见。 */
    bool cursor_visible_ = true;
    /** @brief 光标闪烁计时器。 */
    float cursor_timer_ = 0.0f;
    /** @brief 光标的像素 X 坐标缓存，-1 表示未计算。 */
    float cursor_pixel_x_ = -1.0f;

    /** @brief 是否正在选择文本。 */
    bool selecting_ = false;
    /** @brief 选择锚点行号。 */
    size_t sel_anchor_line_ = 0;
    /** @brief 选择锚点列号。 */
    size_t sel_anchor_col_ = 0;

    /** @brief Shift 键是否被按住。 */
    bool shift_pressed_ = false;

    /** @brief 回车后是否抑制下一个字符输入。 */
    bool suppress_char_after_enter_ = false;

    /** @brief 上次鼠标点击的毫秒时间戳，用于双击检测。 */
    long long last_click_ms_ = 0;
    /** @brief 双击判定时间阈值。 */
    static constexpr long long DBL_CLICK_MS = 500;

    /** @brief 鼠标左键是否按下。 */
    bool mouse_down_ = false;
    /** @brief 拖拽操作是否已确定。 */
    bool drag_resolved_ = false;
    /** @brief 是否即将判定为拖拽。 */
    bool pending_is_drag_ = false;
    /** @brief 是否正在拖拽滚动条滑块。 */
    bool scrollbar_dragging_ = false;
    /** @brief 鼠标是否悬停在滚动条上。 */
    bool scrollbar_hovering_ = false;
    /** @brief 滚动条拖拽起始 Y 坐标。 */
    float scrollbar_drag_start_y_ = 0.0f;
    /** @brief 滚动条拖拽起始滚动值。 */
    float scrollbar_drag_start_scroll_ = 0.0f;

    /** @brief 待处理的点击事件。 */
    struct pending_click {
        float x, y;
        bool shift = false;
        bool dbl = false;
        bool active = false;
    } pending_click_;

    /** @brief 解析并执行待处理的点击事件。 */
    void resolve_click(std::shared_ptr<renderer> r);
    /** @brief 选中光标所在位置的单词。 */
    void select_word_at(size_t line, size_t col, std::string_view sv);

    /** @brief 滚动条滑块背景颜色过渡动画。 */
    color_transition scrollbar_thumb_bg_;

    /** @brief 请求重绘回调。 */
    std::function<void()> repaint_cb_;
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
    /** @brief 不含脏标记的基础标题。 */
    std::string base_title_;
    /** @brief 根据 dirty_ 状态更新标签标题。 */
    void update_title();

    /** @brief 行高。 */
    static constexpr float LINE_HEIGHT = 22.0f;
    /** @brief 行号区域左边界宽度。 */
    static constexpr float LEFT_MARGIN = 48.0f;
    /** @brief 文本右侧内边距。 */
    static constexpr float RIGHT_PAD = 12.0f;
    /** @brief 顶部内边距。 */
    static constexpr float TOP_PAD = 8.0f;
    /** @brief 滚动条宽度。 */
    static constexpr float SCROLLBAR_W = 14.0f;
    /** @brief 光标闪烁周期。 */
    static constexpr float CURSOR_BLINK_MS = 530.0f;
    /** @brief 允许打开的最大文件大小。 */
    static constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;

    /** @brief 绘制编辑器背景。 */
    void draw_background(std::shared_ptr<renderer> r);
    /** @brief 绘制指定行的行号。 */
    void draw_line_numbers(std::shared_ptr<renderer> r, size_t line_idx, float line_y);
    /** @brief 绘制指定行的文本内容。 */
    void draw_line_content(std::shared_ptr<renderer> r, size_t line_idx,
                           float line_y, float avail_width);
    /** @brief 绘制光标。 */
    void draw_cursor(std::shared_ptr<renderer> r);
    /** @brief 绘制选中文本的高亮区域。 */
    void draw_selection(std::shared_ptr<renderer> r);
    /** @brief 绘制垂直滚动条。 */
    void draw_scrollbar(std::shared_ptr<renderer> r);
    /**
     * @brief 判断坐标是否在滚动条区域内。
     * @param px X 坐标
     * @param py Y 坐标
     */
    bool is_on_scrollbar(float px, float py) const;
    /**
     * @brief 判断坐标是否在滚动条滑块上。
     * @param px X 坐标
     * @param py Y 坐标
     */
    bool is_on_scrollbar_thumb(float px, float py) const;
    /**
     * @brief 计算滚动条滑块的位置和大小。
     * @param[out] out_y 滑块顶部 Y 坐标
     * @param[out] out_h 滑块高度
     * @param[out] out_max_scroll 最大滚动值
     */
    void scrollbar_thumb_rect(float& out_y, float& out_h, float& out_max_scroll) const;
    /** @brief 处理键盘事件。 */
    void handle_key(const key_event_data& key);
    /** @brief 将光标限制在有效范围内。 */
    void clamp_cursor();
    /** @brief 滚动视图确保光标可见。 */
    void ensure_cursor_visible();

    /** @brief 删除当前选中的文本。 */
    void delete_selection();
    /**
     * @brief 在光标位置插入一个 Unicode 码点。
     * @param cp Unicode 码点
     */
    void insert_codepoint(unsigned int cp);
    /** @brief 执行退格操作。 */
    void do_backspace();
    /** @brief 执行删除操作。 */
    void do_delete();
    /** @brief 全选。 */
    void handle_select_all();
    /** @brief 复制选中文本到剪贴板。 */
    void handle_copy();
    /** @brief 剪切选中文本到剪贴板。 */
    void handle_cut();
    /** @brief 从剪贴板粘贴文本。 */
    void handle_paste();
    /** @brief 获取当前选中的文本。 */
    std::string get_selected_text() const;
    /** @brief 光标左移一个字符。 */
    void cursor_left();
    /** @brief 光标右移一个字符。 */
    void cursor_right();
    /** @brief 光标上移一行。 */
    void cursor_up();
    /** @brief 光标下移一行。 */
    void cursor_down();
    /** @brief 光标移动到行首。 */
    void cursor_home();
    /** @brief 光标移动到行尾。 */
    void cursor_end();
    /** @brief 光标左移一个单词。 */
    void cursor_word_left();
    /** @brief 光标右移一个单词。 */
    void cursor_word_right();

};

} // namespace edit
} // namespace spiration