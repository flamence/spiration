/**
 * @file text_field.h
 * @brief 单行文本输入框控件。
 * @author clk
 */

#pragma once

#include <spiration/ui/text_area.h>
#include <functional>
#include <string>
#include <unordered_map>

namespace spiration {

/**
 * @brief 单行文本输入框。
 */
class text_field : public text_area {
public:
    text_field() = default;

    std::string placeholder;
    std::function<void(const std::string&)> on_submit;

    /** @brief 是否支持多行输入，启用后 Shift+Enter 插入换行。 */
    bool multiline = false;

    /** @brief 光标自动跟随阈值 */
    float scroll_margin = 30.0f;

    /** @brief 使当前聚焦的 text_field 失焦。组合框/菜单打开时调用。 */
    static void blur_current();
    /** @brief 当前聚焦的 text_field。 */
    static text_field* current_focused();

    bool hit_test(float x, float y) const override;

    void tick(float dt_ms) override;
    void layout() override;

    void handle_event(const event_type& type, void* data) override;

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;

    void focus();
    void blur();
    bool focused() const { return focused_; }

    /// @brief 全选（text_field 用字节偏移光标，需覆写 text_area 的行列版本）。
    void select_all() override;

    /// @brief 清空内容并复位光标与滚动。
    void clear();

    /// @brief 获得焦点（由 focus_manager 驱动）。
    void on_focus() override;
    /// @brief 失去焦点（由 focus_manager 驱动）。
    void on_blur() override;

protected:
    /// @brief 上报光标位置供输入法定位。
    void report_ime_caret() override;

private:
    bool focused_ = false;
    size_t cursor_pos_ = 0;
    size_t sel_anchor_ = 0;
    bool selecting_ = false;
    bool mouse_down_ = false;
    float scroll_x_ = 0.0f;
    /// @brief 多行模式下的垂直滚动偏移。
    float scroll_y_ = 0.0f;
    float cursor_blink_ = 0.0f;
    bool cursor_visible_ = true;
    std::shared_ptr<renderer> cached_renderer_;
    static constexpr float cursor_blink_interval_ = 530.0f;

    /**
     * @brief 选区高亮两段的宽度缓存（槽位 = 行号 * 2、行号 * 2 + 1）。
     * 失效键由 cached_prefix_width 逐项比较：行文本 / 起始列 / 长度 / 字号 / 字体族。
     */
    mutable std::unordered_map<size_t, text_area::prefix_metric> field_selection_metric_cache_;
    /// @brief 光标前缀宽度缓存（槽位 0：行前缀；槽位 1：组合文本前缀）。
    mutable std::unordered_map<size_t, text_area::prefix_metric> field_cursor_metric_cache_;

    float padding_h_ = 8.0f;
    float padding_v_ = 6.0f;

    /// @brief 首行内容 y（未含垂直滚动；多行时顶部对齐，单行保持居中）。
    float content_top() const;

    bool has_selection() const;
    void delete_selection();
    std::string selected_text() const;
    void insert_at_cursor(const std::string& s);
    void delete_before_cursor();
    void delete_after_cursor();
    void move_cursor(int delta);
    void ensure_cursor_visible();
    size_t hit_to_cursor(float mx, float my, std::shared_ptr<renderer> r = nullptr) const;
    void open_context_menu(float mx, float my) override;
};

}
