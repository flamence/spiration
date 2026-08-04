/**
 * @file collapsible.h
 * @brief 可折叠容器控件。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme_manager.h>
#include <utils/animation.h>

#include <functional>
#include <memory>
#include <string>

namespace spiration {

/**
 * @brief 可折叠容器。
 */
class collapsible : public widget {
public:
    collapsible() {
        widget_style.cursor = cursor_type::pointer;
    }

    std::string summary_text;
    bool expanded = false;
    std::function<void(bool)> on_toggle;

    float summary_height = 30.0f;
    float animation_ms = 180.0f;

    /// @brief 内容区最大高度。
    float max_content_height = 0.0f;

    /// @brief 设置 summary 文本。
    void set_summary(const std::string& s) {
        summary_text = s;
        if (request_repaint_) request_repaint_();
    }

    /// @brief 设置内容控件。
    void set_content(std::unique_ptr<widget> w);

    /// @brief 获取内容控件。
    widget* content() const { return content_; }

    void set_expanded(bool v);
    void toggle() { set_expanded(!expanded); }
    bool is_expanded() const { return expanded; }

    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void layout() override;
    size layout_preferred_size() const override;

private:
    widget* content_ = nullptr;
    value_transition content_height_{0.0f};
    color_transition summary_bg_{color::transparent()};
    bool hovered_summary_ = false;
    mutable float content_measured_h_ = 0.0f;

    /// @brief 内容区展开后的目标高度。
    float content_target_height() const;

    void on_hover_change(bool hovered) override;
};

} // namespace spiration
