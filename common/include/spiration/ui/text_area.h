/**
 * @file text_area.h
 * @brief 多行文本编辑区域。
 * @author clk
 */

#pragma once

#include <spiration/ui/label.h>
#include <spiration/ui/theme_manager.h>
#include <spiration/utils/animation.h>
#include <functional>
#include <string>
#include <nonstd/string_view.hpp>
#include <unordered_map>
#include <vector>

namespace spiration {

/**
 * @brief 多行文本编辑器。
 */
class text_area : public label {
public:
    text_area() {
        widget_style.cursor = cursor_style::text;
        focusable = true;
    }

    std::function<void(const std::string&)> on_changed;
    ///< 保存回调。
    std::function<void()> on_save;
    ///< 提交回调（submit_on_enter 时 Enter 触发）。
    std::function<void(const std::string&)> on_submit;

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
    ///< 空文本且未聚焦时显示的占位文本。
    std::string placeholder;
    ///< 是否 Enter 提交（Shift+Enter 仍插入换行）。
    bool submit_on_enter = false;

    bool hit_test(float x, float y) const override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;

    ///< 文本总行数（不含末尾空行）。
    size_t line_count() const;
    ///< 内容按当前行高/内边距计算的自然高度。
    float content_preferred_height() const;

    void focus();
    void blur();
    bool focused() const { return focused_; }

    /// @brief 获得焦点。
    void on_focus() override;
    /// @brief 失去焦点。
    void on_blur() override;

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
    virtual void select_all();
    ///< 复制选中文本到剪贴板。
    void copy();
    ///< 剪切选中文本到剪贴板。
    void cut();
    ///< 从剪贴板粘贴文本。
    void paste();

    ///< 重置光标/选区/滚动（换内容后调用）。
    void reset_view();

    /// @brief 更新输入法组合文本（preedit）。
    /// @param text 组合文本（UTF-8）；空串表示组合结束。
    /// @param cursor 组合内光标（UTF-16 码元数）；-1 表示末尾。
    void set_composition(const std::string& text, int cursor);
    /// @brief 是否处于输入法组合状态。
    bool has_composition() const { return !composition_text_.empty(); }

protected:
    float cursor_pixel_x_ = -1.0f;

    /**
     * @brief 前缀宽度缓存项：避免 paint 期对同一段文本反复 measure。
     * 失效键：行文本快照 line / 起始字节 start / 长度 len / 字号 font_size / 字体族 font_family。
     * 文本编辑会经 rebuild_line_starts() 清空缓存；字号/字体变化则由键比较自然失效。
     */
    struct prefix_metric {
        std::string line;
        size_t start = 0;
        size_t len = 0;
        float width = 0.0f;
        float font_size = -1.0f;
        std::string font_family;
        bool valid = false;
    };

    /**
     * @brief 取 line[start, start+len) 这段文本的像素宽度；键一致时直接复用缓存值。
     * @param cache 缓存表；@param slot 该段文本在缓存表中的槽位
     */
    static float cached_prefix_width(std::unordered_map<size_t, prefix_metric>& cache,
                                     size_t slot, const std::shared_ptr<renderer>& r,
                                     const std::string& line, size_t start, size_t len,
                                     float font_size, const std::string& font_family);

    /// @brief 选中行高亮的两段宽度缓存（槽位 = 行号 * 2 和 行号 * 2 + 1）。
    mutable std::unordered_map<size_t, prefix_metric> selection_metric_cache_;
    /// @brief 光标行前缀宽度 / 输入法组合文本前缀宽度缓存（槽位 0、1）。
    mutable std::unordered_map<size_t, prefix_metric> cursor_metric_cache_;
    /// @brief 行号文本宽度缓存（下标 = 行号 - 1，值为 -1 表示未测量）。
    /// 失效键：font_size - 1、font_family；行号文本只由行号决定，与文本内容无关。
    mutable std::vector<float> line_number_width_cache_;
    mutable float line_number_cache_font_ = -2.0f;
    mutable std::string line_number_cache_family_;
    /// @brief 行号宽度缓存的最大行数（超出后改为逐帧测量，避免超长文档占用过多内存）。
    static constexpr size_t LINE_NUMBER_CACHE_MAX_LINES = 65536;
    /// @brief 选区宽度缓存的最大行数（防止超长文档全选时缓存按行无限膨胀）。
    static constexpr size_t SELECTION_CACHE_MAX_LINES = 8192;

    ///< 输入法组合文本（preedit）与组合内光标（UTF-16 码元数，-1=末尾）。
    std::string composition_text_;
    int composition_cursor_ = -1;

    /// @brief 上报光标位置供输入法定位。
    virtual void report_ime_caret();

    ///< 文本起始 X。
    float text_x() const {
        return (show_line_numbers ? gutter_width : 0.0f) + text_pad_left;
    }

    /// @brief UTF-16 码元偏移 → UTF-8 字节偏移（组合文本内光标换算）。
    static size_t utf16_offset_to_utf8_bytes(const std::string& utf8, int utf16_units);

private:
    std::vector<size_t> line_starts_;
    bool line_starts_dirty_ = true;
    void rebuild_line_starts();
    nonstd::string_view line_text(size_t n) const;

    bool focused_ = false;
    size_t cursor_line_ = 0;
    size_t cursor_col_ = 0;
    float cursor_timer_ = 0.0f;
    bool cursor_visible_ = true;
    static constexpr float CURSOR_BLINK_MS = 530.0f;

    float scroll_y_ = 0.0f;
    float scroll_x_ = 0.0f;

    bool selecting_ = false;
    size_t sel_anchor_line_ = 0;
    size_t sel_anchor_col_ = 0;
    bool shift_pressed_ = false;

    bool mouse_down_ = false;
    float press_x_ = 0.0f;
    float press_y_ = 0.0f;
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

    ///< 水平滚动条是否可见。
    bool h_scroll_visible() const { return scroll_max_x() > 0.0f; }

    void open_context_menu(float mx, float my) override;

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
    void select_word_at(size_t line, size_t col, nonstd::string_view sv);
    void resolve_position(float px, float py, size_t& out_line, size_t& out_col,
                          std::shared_ptr<renderer> r) const;
    void resolve_click(std::shared_ptr<renderer> r);
};

}

