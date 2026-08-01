/**
 * @file ohos_application.h
 * @brief HarmonyOS 平台 application 特化。
 * @author clk
 */

#pragma once

#include <extension/extension_manager.h>
#include <memory>
#include <ui/root.h>
#include <utils/platform.h>
#include <window/window.h>

namespace spiration {

// 前向声明
class widget;

/**
 * @brief HarmonyOS 平台 application 实现，支持延迟窗口创建。
 */
class ohos_application {
public:
    static ohos_application* instance();

    /// 初始化扩展管理器（不创建窗口）
    void initialize_early();

    /// 设置窗口并创建 root widget
    void set_window(std::shared_ptr<window> window, int32_t width = 0, int32_t height = 0);

    /// 初始化 normal 阶段扩展
    void initialize_normal();

    /// 单次循环迭代（由 onFrameTick 驱动）
    void tick(float dt_ms);

    /// 关闭应用
    void shutdown();

    extension_manager* extension() const { return &extension_manager::instance(); }
    window* get_window() const { return window_.get(); }
    widget* get_widget() const { return widget_; }

private:
    ohos_application() = default;
    ~ohos_application() = default;
    ohos_application(const ohos_application&) = delete;
    ohos_application& operator=(const ohos_application&) = delete;

    static ohos_application* instance_;
    std::shared_ptr<window> window_;
    widget* widget_ = nullptr;
};

} // namespace spiration
