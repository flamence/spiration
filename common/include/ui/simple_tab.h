/**
 * @file simple_tab.h
 * @brief 可自定义绘制回调的简易标签页，供扩展使用。
 * @author clk
 */

#pragma once

#include <ui/tab_bar.h>
#include <renderer/renderer.h>
#include <window/event.h>
#include <functional>

namespace spiration {

/**
 * @brief 简易标签页，扩展可通过 paint 回调自定义绘制内容。
 */
class simple_tab : public tab {
public:
    using paint_fn = std::function<void(std::shared_ptr<renderer>,
                                        float x, float y,
                                        float w, float h)>;

    /**
     * @param title 标签标题
     * @param painter 绘制回调
     */
    explicit simple_tab(const std::string& title, paint_fn painter)
        : painter_(std::move(painter)) {
        title_ = title;
        widget_style.background_color = {0.2f, 0.2f, 0.25f};
    }

    void paint(std::shared_ptr<renderer> renderer) override {
        if (painter_) {
            painter_(renderer, x, y, width, height);
        }
    }

private:
    paint_fn painter_;
};

} // namespace spiration
