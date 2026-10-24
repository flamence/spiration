/**
 * @file collapsible.h
 * @brief 可折叠容器控件。
 * @author clk
 */

#pragma once

#include <spiration/ui/widget.h>
#include <spiration/ui/theme_manager.h>
#include <spiration/utils/animation.h>

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
        widget_style.cursor = cursor_style::pointer;
        focusable = true;
    }

    std::string summary_text;
    bool expanded = false;
    std::function<void(bool)> on_toggle;

    /// @brief 折叠动画完成且已折叠时回调（用于外部在收起后隐藏，如清空待办）。
    std::function<void()> on_collapsed;

    /// @brief 是否在摘要栏右侧显示操作按钮（如任务列表的清除）。
    bool show_summary_action = false;
    /// @brief 点击摘要栏右侧操作按钮时回调。
    std::function<void()> on_summary_action;

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
    /// @brief 当前显示高度，平滑跟随目标高度（内容流式增长时不跳变）。
    value_transition height_anim_{0.0f};
    /// @brief 切换动画期间的内容目标高度：内容（流式输出等）增长时平滑追向，
    ///        避免 height = progress * target 中 target 跳变造成可见 step。
    value_transition target_h_anim_{0.0f};
    /// @brief 目标高度是否已按首次实测值初始化（避免切换开始时陈旧/零值跳变）。
    bool target_h_measured_ = false;
    color_transition summary_bg_{color::transparent()};
    bool hovered_summary_ = false;
    bool hover_action_ = false;
    /// @brief 内容实际高度；-1 表示尚未测量（首次布局前）。
    mutable float content_measured_h_ = -1.0f;

    /// @brief 摘要栏右侧操作按钮区域（本控件坐标）。
    rectangle action_rect() const {
        return {width - 30.0f, (summary_height - 22.0f) * 0.5f, 22.0f, 22.0f};
    }

    /// @brief 内容区展开后的目标高度。
    float content_target_height() const;

    void on_hover_change(bool hovered) override;
};

} // namespace spiration
