/**
 * @file progress_bar.h
 * @brief 进度条控件，显示完成进度。
 * @author clk
 */

#pragma once

#include <ui/widget.h>
#include <ui/theme_manager.h>

namespace spiration {

/**
 * @brief 水平进度条，显示 0..1 范围的进度。
 */
class progress_bar : public widget {
public:
    float progress = 0.0f;
    float bar_height = 8.0f;
    float corner_radius = 4.0f;

    color bg_color = theme_manager::get(theme_manager::PROGRESS_BG);
    color fill_color = theme_manager::get(theme_manager::PROGRESS_FILL);

    void paint(std::shared_ptr<renderer> renderer) override;

    size layout_preferred_size() const override;
};

}
