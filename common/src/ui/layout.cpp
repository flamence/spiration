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
        if (child->widget_style.width > 0) {
            fixedTotal += static_cast<float>(child->widget_style.width);
        } else {
            ++flexCount;
        }
    }

    float totalSpacing = spacing_ * std::max(0.0f, static_cast<float>(children.size()) - 1.0f);
    float flexAvailable = bounds.width - fixedTotal - totalSpacing;
    if (flexAvailable < 0.0f) flexAvailable = 0.0f;

    float currentX = bounds.x;
    for (auto& child : children) {
        child->y = bounds.y;
        child->height = bounds.height;

        if (child->widget_style.width > 0) {
            child->x = currentX;
            child->width = static_cast<float>(child->widget_style.width);
            currentX += child->width + spacing_;
        } else {
            float flexW = (flexCount > 0) ? (flexAvailable / flexCount) : 0.0f;
            child->x = currentX;
            child->width = flexW;
            currentX += flexW + spacing_;
            --flexCount;
            flexAvailable -= flexW;
        }
    }
}

size horizontal_layout::preferred_size(
    const std::vector<std::unique_ptr<widget>>& children) const
{
    float w = 0.0f, h = 0.0f;
    bool first = true;
    for (const auto& child : children) {
        size pref = child->layout_preferred_size();
        w += pref.width;
        if (first) {
            h = pref.height;
            first = false;
        } else {
            h = std::max(h, pref.height);
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

    float fixedTotal = 0.0f;
    int flexCount = 0;
    for (const auto& child : children) {
        if (child->widget_style.height > 0) {
            fixedTotal += static_cast<float>(child->widget_style.height);
        } else {
            ++flexCount;
        }
    }

    float totalSpacing = spacing_ * std::max(0.0f, static_cast<float>(children.size()) - 1.0f);
    float flexAvailable = bounds.height - fixedTotal - totalSpacing;
    if (flexAvailable < 0.0f) flexAvailable = 0.0f;

    float currentY = bounds.y;
    for (auto& child : children) {
        child->x = bounds.x;
        child->width = bounds.width;

        if (child->widget_style.height > 0) {
            child->y = currentY;
            child->height = static_cast<float>(child->widget_style.height);
            currentY += child->height + spacing_;
        } else {
            float flexH = (flexCount > 0) ? (flexAvailable / flexCount) : 0.0f;
            child->y = currentY;
            child->height = flexH;
            currentY += flexH + spacing_;
            --flexCount;
            flexAvailable -= flexH;
        }
    }
}

size vertical_layout::preferred_size(
    const std::vector<std::unique_ptr<widget>>& children) const
{
    float w = 0.0f, h = 0.0f;
    bool first = true;
    for (const auto& child : children) {
        size pref = child->layout_preferred_size();
        h += pref.height;
        if (first) {
            w = pref.width;
            first = false;
        } else {
            w = std::max(w, pref.width);
        }
    }
    h += spacing_ * std::max(0.0f, static_cast<float>(children.size()) - 1.0f);
    return {w, h};
}

} 
