/**
 * @file focus_manager.h
 * @brief 全局焦点管理器。
 * @author clk
 */

#pragma once

namespace spiration {

class widget;

/**
 * @brief 全局焦点管理器：同一时刻只有一个可交互控件持有焦点。
 */
class focus_manager {
public:
    /// @brief 单例访问。
    static focus_manager& instance();

    /// @brief 请求将焦点给 w。旧焦点自动失焦。
    void request_focus(widget* w);

    /// @brief 清除焦点。
    void clear_focus();

    /// @brief 当前持有焦点的控件，可能为 nullptr。
    widget* focused() const { return focused_; }

    /// @brief 控件销毁时清理。
    void on_widget_destroyed(widget* w);

private:
    focus_manager() = default;
    widget* focused_ = nullptr;
};

}
