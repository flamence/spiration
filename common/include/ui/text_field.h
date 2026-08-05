/**
 * @file text_field.h
 * @brief 单行文本输入框控件。
 * @author clk
 */

#pragma once

#include <ui/text_area.h>
#include <functional>
#include <string>

namespace spiration {

/**
 * @brief 单行文本输入框。
 */
class text_field : public text_area {
public:
    text_field() = default;

    std::string placeholder;
    std::function<void(const std::string&)> on_submit;

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

    /// @brief 获得焦点（由 focus_manager 驱动）。
    void on_focus() override;
    /// @brief 失去焦点（由 focus_manager 驱动）。
    void on_blur() override;

private:
    bool focused_ = false;
    size_t cursor_pos_ = 0;
    size_t sel_anchor_ = 0;
    bool selecting_ = false;
    bool mouse_down_ = false;
    float scroll_x_ = 0.0f;
    float cursor_blink_ = 0.0f;
    bool cursor_visible_ = true;
    std::shared_ptr<renderer> cached_renderer_;
    static constexpr float cursor_blink_interval_ = 530.0f;

    float padding_h_ = 8.0f;
    float padding_v_ = 6.0f;

    bool has_selection() const;
    void delete_selection();
    std::string selected_text() const;
    void insert_at_cursor(const std::string& s);
    void delete_before_cursor();
    void delete_after_cursor();
    void move_cursor(int delta);
    void ensure_cursor_visible();
    size_t hit_to_cursor(float mx, std::shared_ptr<renderer> r = nullptr) const;
    void open_context_menu(float mx, float my) override;
};

}
