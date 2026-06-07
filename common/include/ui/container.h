/**
 * @file container.h
 * @brief Container 容器控件，可容纳子 widget 并绘制背景。
 * @author clk
 */

#pragma once

#include "widget.h"
#include <ui/layout.h>
#include <memory>

namespace spiration {

/**
 * @brief 容器控件，可包含子 widget 并绘制背景矩形。
 *
 * container 是最基础的组合控件，负责：
 * - 绘制背景颜色区域
 * - 作为子控件的布局容器，可设置 layout 管理器自动排列子 widget
 *
 * 若未设置 layout，子 widget 保持手动定位（兼容旧行为）。
 */
class container : public widget {
public:
    void layout() override;

    void paint(std::shared_ptr<renderer> renderer) override;

    /**
     * @brief 设置布局管理器，接管子 widget 的排列。
     */
    void set_layout_manager(std::unique_ptr<layout_manager> lm) {
        layout_manager_ = std::move(lm);
    }

    /**
     * @brief 获取当前布局管理器。
     */
    layout_manager* get_layout_manager() const { return layout_manager_.get(); }

private:
    std::unique_ptr<layout_manager> layout_manager_ = nullptr;
};

}