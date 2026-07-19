/**
 * @file text_field.h
 * @brief 单行文本输入框控件。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme.h>
#include <functional>
#include <string>

namespace spiration {

/**
 * @brief 单行文本输入框，支持键盘输入、光标移动、文本选择。
 */
class text_field : public widget {
public:
    std::string text;
    std::string placeholder;
    float font_size = 14.0f;
    std::function<void(const std::string&)> on_changed;
    std::function<void(const std::string&)> on_submit;

    bool hit_test(float x, float y) const override;

    void tick(float dt_ms) override;

    void handle_event(const event_type& type, void* data) override;

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;

    void focus();
    void blur();
    bool focused() const { return focused_; }

private:
    bool focused_ = false;
    size_t cursor_pos_ = 0;
    float cursor_blink_ = 0.0f;
    bool cursor_visible_ = true;
    static constexpr float cursor_blink_interval_ = 530.0f;

    float padding_h_ = 8.0f;
    float padding_v_ = 6.0f;

    void insert_at_cursor(const std::string& s);
    void delete_before_cursor();
    void delete_after_cursor();
    void move_cursor(int delta);
    size_t hit_to_cursor(float mx) const;
};

}
