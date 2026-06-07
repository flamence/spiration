/**
 * @file spacer.h
 * @brief 弹性空间控件，用于布局中占用剩余空间。
 * @author clk
 */

#pragma once

#include <ui/widget.h>

namespace spiration {

/**
 * @brief 弹性空间控件。
 *
 * spacer 在水平或垂直布局中占据剩余空间，不绘制任何内容。
 * 在无边框窗口中，spacer 区域可作为窗口拖拽手柄使用（不消费鼠标事件，可穿透触发拖拽）。
 *
 * 用法：
 * @code
 * auto sp = std::make_unique<spacer>();
 * sp->style.width = 1;  // 布局权重，占用剩余空间
 * container.add_child(std::move(sp));
 * @endcode
 */
class spacer : public widget {
public:
    void paint(std::shared_ptr<renderer> renderer) override {
        
    }
};

} 
