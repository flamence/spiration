/**
 * @file tab_head_item.h
 * @brief 标签头项控件定义。
 * @author clk
 */

#pragma once

#include <spiration/ui/widget.h>
#include <spiration/utils/animation.h>
#include <functional>
#include <string>

namespace spiration {

class tab_bar;

/**
 * @brief 标签头项，代表单个 tab 在标签栏中的标题按钮。
 */
class tab_head_item : public widget {
    friend class tab_bar;
public:
    explicit tab_head_item(const std::string& title);

    void set_on_activate(std::function<void()> cb) { on_activate_ = std::move(cb); }
    void set_on_close(std::function<void()> cb) { on_close_ = std::move(cb); }

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void tick(float dt_ms) override;

    void set_active(bool active) { active_ = active; }
    bool is_active() const { return active_; }
    void set_title(const std::string& t) { title_ = t; }
    const std::string& get_title() const { return title_; }

private:
    std::string title_;
    bool active_ = false;
    bool hovering_ = false;
    bool close_hovering_ = false;

    color_transition bg_;
    color_transition text_;
    color_transition close_fg_;

    std::function<void()> on_activate_;
    std::function<void()> on_close_;

    void sync_colors();

    static constexpr float CLOSE_BTN_W = 20.0f;
    static constexpr float CLOSE_ICON_SIZE = 10.0f;
};

} // namespace spiration
