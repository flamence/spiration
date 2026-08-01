/**
 * @file layout.cpp
 * @brief 布局管理器实现。
 * @author clk
 */

#include <ui/layout.h>
#include <ui/widget.h>
#include <cassert>
#include <algorithm>
#include <numeric>

namespace spiration {

void horizontal_layout::arrange(
    const rectangle& bounds,
    const std::vector<std::unique_ptr<widget>>& children) const
{
    if (children.empty()) return;

    
    float fixedTotal = 0.0f;
    int flexCount = 0;
    for (const auto& child : children) {
        float mlr = static_cast<float>(child->widget_style.margin.left +
                                       child->widget_style.margin.right);
        if (child->widget_style.width > 0) {
            fixedTotal += static_cast<float>(child->widget_style.width) + mlr;
        } else {
            ++flexCount;
        }
    }

    float totalSpacing = spacing_ * std::max(0.0f, static_cast<float>(children.size()) - 1.0f);
    float flexAvailable = bounds.width - fixedTotal - totalSpacing;
    if (flexAvailable < 0.0f) flexAvailable = 0.0f;
    float flexW = (flexCount > 0) ? (flexAvailable / static_cast<float>(flexCount)) : 0.0f;

    float currentX = bounds.x;
    for (auto& child : children) {
        const auto& m = child->widget_style.margin;
        float ml = static_cast<float>(m.left);
        float mr = static_cast<float>(m.right);
        float mt = static_cast<float>(m.top);
        float mb = static_cast<float>(m.bottom);
        child->y = bounds.y + mt;
        child->height = std::max(0.0f, bounds.height - mt - mb);

        if (child->widget_style.width > 0) {
            child->x = currentX + ml;
            child->width = static_cast<float>(child->widget_style.width);
            currentX += child->width + ml + mr + spacing_;
        } else {
            child->x = currentX + ml;
            child->width = flexW;
            currentX += flexW + ml + mr + spacing_;
        }
    }
}

size horizontal_layout::preferred_size(
    const std::vector<std::unique_ptr<widget>>& children) const
{
    float w = 0.0f, h = 0.0f;
    bool first = true;
    for (const auto& child : children) {
        const auto& m = child->widget_style.margin;
        size pref = child->layout_preferred_size();
        w += pref.width + static_cast<float>(m.left + m.right);
        float ch = pref.height + static_cast<float>(m.top + m.bottom);
        if (first) {
            h = ch;
            first = false;
        } else {
            h = std::max(h, ch);
        }
    }
    w += spacing_ * std::max(0.0f, static_cast<float>(children.size()) - 1.0f);
    return {w, h};
}

void vertical_layout::arrange(
    const rectangle& bounds,
    const std::vector<std::unique_ptr<widget>>& children) const
{
    if (children.empty()) return;

    float currentY = bounds.y;
    for (auto& child : children) {
        const auto& m = child->widget_style.margin;
        float ml = static_cast<float>(m.left);
        float mr = static_cast<float>(m.right);
        float mt = static_cast<float>(m.top);
        float mb = static_cast<float>(m.bottom);

        child->x = bounds.x + ml;
        child->width = std::max(0.0f, bounds.width - ml - mr);

        if (child->widget_style.height > 0) {
            child->y = currentY + mt;
            child->height = static_cast<float>(child->widget_style.height);
            currentY += child->height + mt + mb + spacing_;
        } else {
            // 先设宽度，再调用 layout() 让子 widget 自行更新高度
            child->layout();
            if (child->height <= 0.0f) {
                size pref = child->layout_preferred_size();
                child->height = pref.height;
            }
            child->y = currentY + mt;
            currentY += child->height + mt + mb + spacing_;
        }
    }
}

size vertical_layout::preferred_size(
    const std::vector<std::unique_ptr<widget>>& children) const
{
    float w = 0.0f, h = 0.0f;
    bool first = true;
    for (const auto& child : children) {
        const auto& m = child->widget_style.margin;
        float ml = static_cast<float>(m.left);
        float mr = static_cast<float>(m.right);
        float mt = static_cast<float>(m.top);
        float mb = static_cast<float>(m.bottom);
        float dh;
        if (child->widget_style.height > 0) {
            dh = static_cast<float>(child->widget_style.height);
        } else {
            size pref = child->layout_preferred_size();
            dh = pref.height;
        }
        h += dh + mt + mb;
        float cw = (child->widget_style.width > 0)
                    ? static_cast<float>(child->widget_style.width)
                    : child->layout_preferred_size().width;
        cw += ml + mr;
        if (first) {
            w = cw;
            first = false;
        } else {
            w = std::max(w, cw);
        }
    }
    h += spacing_ * std::max(0.0f, static_cast<float>(children.size()) - 1.0f);
    return {w, h};
}

} // namespace spiration
