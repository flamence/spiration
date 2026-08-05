/**
 * @file todo_view.h
 * @brief 待办事项可视化控件。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/tool/todo_tool.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <ui/widget.h>

namespace spiration {
namespace agent {

/**
 * @brief 待办事项视图。
 */
class todo_view : public widget {
public:
    todo_view();

    /// @brief 内容变化时回调。
    std::function<void()> on_content_changed;

    /// @brief 最大可视高度。
    float max_height = 200.0f;

    void tick(float dt_ms) override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void layout() override;
    size layout_preferred_size() const override;
    void handle_event(const event_type& type, void* data) override;

private:
    std::vector<todo_item> items_;
    uint64_t seen_version_ = 0;
    int hovered_row_ = -1;
    /// @brief 内部垂直滚动偏移。
    float scroll_y_ = 0.0f;
    /// @brief 最大滚动距离。
    float scroll_max_y() const {
        return std::max(0.0f, desired_height() - height);
    }

    /// @brief 重新读取 store 并触发刷新。
    void refresh();
    /// @brief 按当前列表计算期望高度。
    float desired_height() const;
};

} // namespace agent
} // namespace spiration
