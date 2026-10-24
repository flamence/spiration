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

    /// @brief 右键菜单回调，参数为窗口坐标。
    std::function<void(float, float)> on_context_menu;

    void paint(std::shared_ptr<renderer> renderer) override;
    void handle_event(const event_type& type, void* data) override;
    void tick(float dt_ms) override;

    void set_active(bool active);
    bool is_active() const { return active_; }
    void set_title(const std::string& t);
    const std::string& get_title() const { return title_; }

protected:
    void on_hover_change(bool hovered) override;

private:
    std::string title_;
    bool active_ = false;
    bool close_hovering_ = false;

    /// @brief 标题截断缓存：仅 title/宽度变化时重新测量。
    mutable std::string display_title_;
    mutable float display_key_width_ = -1.0f;
    mutable std::string display_key_title_;

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
