/**
 * @file text_area.h
 * @brief 多行文本编辑区域，支持滚动、光标、选区。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme.h>
#include <functional>
#include <string>
#include <vector>

namespace spiration {

/**
 * @brief 多行文本编辑器。
 */
class text_area : public widget {
public:
    std::string text;
    std::function<void(const std::string&)> on_changed;
    float font_size = 14.0f;
    float line_height = 20.0f;

    bool hit_test(float x, float y) const override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    size layout_preferred_size() const override;

    void focus();
    void blur();
    bool focused() const { return focused_; }
    float scroll_y() const { return scroll_y_; }

private:
    bool focused_ = false;
    float scroll_y_ = 0.0f;
    float content_height_ = 0.0f;
    size_t cursor_line_ = 0;
    size_t cursor_col_ = 0;
    float cursor_blink_ = 0.0f;
    bool cursor_visible_ = true;
    static constexpr float cursor_blink_interval_ = 530.0f;
    float padding_h_ = 8.0f;
    float padding_v_ = 6.0f;

    void insert_at_cursor(const std::string& s);
    void delete_before_cursor();
    void delete_after_cursor();
    void new_line();
    void move_cursor_h(int delta);
    void move_cursor_v(int delta);

    struct line_info { size_t start; size_t length; };
    std::vector<line_info> lines_;
    void rebuild_lines();
    size_t line_end(size_t line_idx) const;
    float line_start_x(size_t line_idx) const;
    void clamp_cursor();
    void ensure_cursor_visible();
};

}
