/**
 * @file application.h
 * @brief 应用定义。
 * @author clk
 */

#pragma once

#include <extension/extension_manager.h>
#include <memory>
#include <ui/root.h>
#include <utils/platform.h>
#include <window/window.h>

namespace spiration {

class application {
public:
    void initialize();

    /**
     * @brief 初始化扩展管理器并完成 early 阶段初始化。
     */
    void initialize_early();

    /**
     * @brief 初始化 normal 阶段扩展。
     * @note 需在窗口与根控件就绪后调用。
     */
    void initialize_normal();

    spiration::extension_manager* extension() const;
    static application* instance();

    /**
     * @brief 附加外部创建的窗口并创建根控件。
     * @param window 窗口实例
     * @param width  逻辑宽度
     * @param height 逻辑高度
     */
    void set_window(std::shared_ptr<spiration::window> window,
                    int32_t width = 0, int32_t height = 0);

    void loop();

    /**
     * @brief 驱动根控件动画等每帧逻辑。
     * @param dt_ms 距离上一帧的毫秒数
     */
    void tick(float dt_ms);

    void shutdown();
    spiration::widget* widget() const;
    spiration::window* window() const;

private:
    static std::unique_ptr<application> instance_;
    std::shared_ptr<spiration::window> window_;
    spiration::widget* widget_;

    std::shared_ptr<spiration::window> create_window();
};

}