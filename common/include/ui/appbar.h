/**
 * @file appbar.h
 * @brief 应用顶部菜单栏控件。
 * @author clk
 */

#pragma once

#include <ui/container.h>

namespace spiration {

/**
 * @brief 应用顶部的菜单栏，包含文件操作等按钮。
 */
class appbar : public container {
public:
    void init() override;

    void layout() override;
};

}