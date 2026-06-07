/**
 * @file layout.h
 * @brief 布局管理器：自动排列子 widget 的算法基类与常见实现。
 * @author clk
 */

#pragma once

#include <memory>
#include <vector>
#include <ui/point.h>
#include <ui/rectangle.h>
#include <ui/size.h>

namespace spiration {


class widget;

/**
 * @brief 布局方向枚举。
 */
enum class layout_direction {
    horizontal, 
    vertical,   
};

/**
 * @brief 布局管理器基类。
 *
 * 由 container 持有，在 layout() 阶段调用 arrange() 来设置子 widget 的位置和尺寸。
 */
class layout_manager {
public:
    virtual ~layout_manager() = default;

    /**
     * @brief 在给定边界内排列子 widget。
     * @param bounds  当前容器的可用区域（位置 + 尺寸）
     * @param children 子 widget 列表（可修改其 x/y/width/height）
     */
    virtual void arrange(const rectangle& bounds,
                         const std::vector<std::unique_ptr<widget>>& children) const = 0;

    /**
     * @brief 计算此布局下所有子 widget 的理想尺寸。
     */
    virtual size preferred_size(const std::vector<std::unique_ptr<widget>>& children) const = 0;
};



/**
 * @brief 水平布局：子 widget 从左到右依次排列。
 *
 * 每个子 widget 的宽度由比例权重 (weight) 控制，
 * 高度填满容器。默认等宽。
 */
class horizontal_layout : public layout_manager {
public:
    explicit horizontal_layout(float spacing = 0.0f) : spacing_(spacing) {}

    void arrange(const rectangle& bounds,
                 const std::vector<std::unique_ptr<widget>>& children) const override;

    size preferred_size(const std::vector<std::unique_ptr<widget>>& children) const override;

    void set_spacing(float spacing) { spacing_ = spacing; }
    float spacing() const { return spacing_; }

private:
    float spacing_ = 0.0f;
};

/**
 * @brief 垂直布局：子 widget 从上到下依次排列。
 *
 * 每个子 widget 的高度由比例权重控制，宽度填满容器。
 */
class vertical_layout : public layout_manager {
public:
    explicit vertical_layout(float spacing = 0.0f) : spacing_(spacing) {}

    void arrange(const rectangle& bounds,
                 const std::vector<std::unique_ptr<widget>>& children) const override;

    size preferred_size(const std::vector<std::unique_ptr<widget>>& children) const override;

    void set_spacing(float spacing) { spacing_ = spacing; }
    float spacing() const { return spacing_; }

private:
    float spacing_ = 0.0f;
};

} 
