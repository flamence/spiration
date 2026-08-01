/**
 * @file text_area.h
 * @brief 多行文本编辑区域。
 * @author clk
 */

#pragma once

#include <ui/label.h>
#include <ui/theme_manager.h>
#include <utils/animation.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace spiration {

/**
 * @brief 多行文本编辑器。
 */
class text_area : public label {
public:
    text_area() {
        widget_style.cursor = cursor_type::text;
    }

    std::function<void(const std::string&)> on_changed;
    ///< 保存回调。
    std::function<void()> on_save;

    ///< 行高。
    float line_height = 22.0f;
    ///< 字体族。
    std::string font_family = "Consolas";
    ///< 是否显示左侧行号栏。
    bool show_line_numbers = false;
    ///< 行号栏宽度。
    float gutter_width = 48.0f;
    ///< 文本区左内边距。
    float text_pad_left = 8.0f;
    ///< 文本区右内边距。
    float text_pad_right = 12.0f;
    ///< 文本区顶部内边距。
    float text_pad_top = 8.0f;
    ///< 滚动条宽度。
    float scroll_bar_width = 14.0f;

    bool hit_test(float x, float y) const override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;

    void focus();
    void blur();
    bool focused() const { return focused_; }

    ///< 当前垂直滚动偏移。
    float scroll_y() const { return scroll_y_; }
    ///< 当前水平滚动偏移。
    float scroll_x() const { return scroll_x_; }
    ///< 垂直滚动到指定偏移。
    void scroll_to_y(float y);
    ///< 水平滚动到指定偏移。
    void scroll_to_x(float x);
    ///< 水平最大滚动量。
    float scroll_max_x() const;

    ///< 光标行号。
    size_t cursor_line() const { return cursor_line_; }
    ///< 光标列号。
    size_t cursor_col() const { return cursor_col_; }

    ///< 是否处于选择状态。
    bool selecting() const { return selecting_; }
    ///< 是否有非空选择。
    bool has_selection() const;
    ///< 获取选中文本。
    std::string selected_text() const;
    ///< 全选。
    void select_all();
    ///< 复制选中文本到剪贴板。
    void copy();
    ///< 剪切选中文本到剪贴板。
    void cut();
    ///< 从剪贴板粘贴文本。
    void paste();

    ///< 重置光标/选区/滚动（换内容后调用）。
    void reset_view();

private:
    std::vector<size_t> line_starts_;
    bool line_starts_dirty_ = true;
    void rebuild_line_starts();
    size_t line_count() const;
    std::string_view line_text(size_t n) const;

    bool focused_ = false;
    size_t cursor_line_ = 0;
    size_t cursor_col_ = 0;
    float cursor_timer_ = 0.0f;
    bool cursor_visible_ = true;
    static constexpr float CURSOR_BLINK_MS = 530.0f;
    float cursor_pixel_x_ = -1.0f;

    float scroll_y_ = 0.0f;
    float scroll_x_ = 0.0f;

    bool selecting_ = false;
    size_t sel_anchor_line_ = 0;
    size_t sel_anchor_col_ = 0;
    bool shift_pressed_ = false;

    bool mouse_down_ = false;
    bool drag_resolved_ = false;
    bool pending_is_drag_ = false;
    bool suppress_char_after_enter_ = false;
    long long last_click_ms_ = 0;
    static constexpr long long DBL_CLICK_MS = 500;
    struct pending_click {
        float x = 0.0f, y = 0.0f;
        bool shift = false;
        bool dbl = false;
        bool active = false;
    } pending_click_;

    bool scrollbar_dragging_ = false;
    bool scrollbar_hovering_ = false;
    float scrollbar_drag_start_y_ = 0.0f;
    float scrollbar_drag_start_scroll_ = 0.0f;
    color_transition scrollbar_thumb_bg_{{0.45f, 0.45f, 0.45f, 0.5f}};

    bool max_width_dirty_ = true;
    float max_line_width_ = 0.0f;
    float compute_max_line_width(std::shared_ptr<renderer> r);

    bool h_scrollbar_dragging_ = false;
    bool h_scrollbar_hovering_ = false;
    float h_scrollbar_drag_start_x_ = 0.0f;
    float h_scrollbar_drag_start_scroll_ = 0.0f;
    color_transition h_scrollbar_thumb_bg_{{0.45f, 0.45f, 0.45f, 0.5f}};

    bool is_on_h_scrollbar_thumb(float px, float py) const;
    void h_scrollbar_thumb_rect(float& out_x, float& out_w, float& out_max_scroll) const;

    void draw_background(std::shared_ptr<renderer> r);
    void draw_gutter(std::shared_ptr<renderer> r);
    void draw_line_numbers(std::shared_ptr<renderer> r, size_t line_idx, float line_y);
    void draw_line_content(std::shared_ptr<renderer> r, size_t line_idx, float line_y);
    void draw_cursor(std::shared_ptr<renderer> r);
    void draw_selection(std::shared_ptr<renderer> r);
    void draw_scrollbar(std::shared_ptr<renderer> r);

    ///< 文本起始 X。
    float text_x() const {
        return (show_line_numbers ? gutter_width : 0.0f) + text_pad_left;
    }

    ///< 水平滚动条是否可见。
    bool h_scroll_visible() const { return scroll_max_x() > 0.0f; }

    void open_context_menu(float mx, float my);

    bool is_on_scrollbar(float px, float py) const;
    bool is_on_scrollbar_thumb(float px, float py) const;
    void scrollbar_thumb_rect(float& out_y, float& out_h, float& out_max_scroll) const;

    void handle_key(const key_event_data& key);
    void clamp_cursor();
    void ensure_cursor_visible();
    void delete_selection();
    void insert_codepoint(unsigned int cp);
    void do_backspace();
    void do_delete();
    void handle_select_all();
    void handle_copy();
    void handle_cut();
    void handle_paste();
    std::string get_selected_text() const;
    void cursor_left();
    void cursor_right();
    void cursor_up();
    void cursor_down();
    void cursor_home();
    void cursor_end();
    void cursor_word_left();
    void cursor_word_right();
    void select_word_at(size_t line, size_t col, std::string_view sv);
    void resolve_click(std::shared_ptr<renderer> r);
};

}

