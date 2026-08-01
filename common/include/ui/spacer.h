/**
 * @file spacer.h
 * @brief 弹性空间控件。
 * @author clk
 */

#pragma once

#include <ui/widget.h>

namespace spiration {

/**
 * @brief 弹性空间控件。
 */
class spacer : public widget {
public:
    void paint(std::shared_ptr<renderer> renderer) override {
        (void)renderer;
    }
};

} 
