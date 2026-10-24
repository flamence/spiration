/**
 * @file application.h
 * @brief 应用定义。
 * @author clk
 */

#pragma once

#include <ghc/filesystem.hpp>
#include <memory>
#include <spiration/logging/logger.hpp>
#include <spiration/extension/extension_manager.h>
#include <spiration/init_phase.h>
#include <spiration/window/window.h>
#include <string>
#include <tl/optional.hpp>

namespace spiration {

class extension_manager;
class task_dispatcher;

/**
 * @brief 应用类。
 */
class application {
public:
    static const std::string APPLICATION_NAME; ///< 应用名称。
    static const std::string APPLICATION_LOWER_NAME; ///< 应用小写名。
    static const std::string APPLICATION_PACKAGE; ///< 应用包名。

#ifdef __OHOS__
    /**
     * @brief 绑定应用至窗口。
     * @param window 目标窗口指针。
     */
    void bind_window(std::shared_ptr<spiration::window> window);
#endif

    /**
     * @return 数据目录。
     */
    ghc::filesystem::path data_directory(tl::optional<std::string> sub = tl::nullopt);

    /**
     * @return 该应用附加的拓展管理器实例。
     */
    std::shared_ptr<spiration::extension_manager> extension() const;

    /**
     * @return 该应用的任务调度器实例。
     */
    task_dispatcher* dispatcher() const;
    
    ~application();
    /**
     * @brief 初始化应用。
     * @param phase 指定所执行应用初始化阶段。
     * @note 对于鸿蒙平台，默认禁止了对窗口的创建，原因是其窗口伴随鸿蒙应用的生命周期。
     */
    bool initialize(tl::optional<init_phase> phase = tl::nullopt);
    
    /**
     * @return 应用实例。
     */
    static application* instance();
    
    /**
     * @return 日志器。
     */
    std::shared_ptr<logging::logger> logger() const;

    /**
     * @brief 执行应用循环。
     */
    void loop();
    
    /**
     * @brief 关闭应用。
     */
    void shutdown();
    
    /**
     * @brief 推进应用工作。
     * @param deltatime 增量时间，以毫秒为单位。
     */
    void tick(float deltatime);
    
    /**
     * @return 该应用所绑定的根控件。
     */
    spiration::widget* widget() const;
    
    /**
     * @return 该应用所绑定的窗口。
     */
    spiration::window* window() const;
    
private:
    /**
     * @brief 构造应用实例。
     * @note 日志器随实例一同创建，确保在早期初始化阶段之前访问日志器时不会得到空指针
     *       （鸿蒙 NAPI 模块注册阶段即会添加 hilog 接收器）。
     */
    application();

    std::shared_ptr<extension_manager> extension_manager_;
    std::unique_ptr<task_dispatcher> dispatcher_;
    static std::unique_ptr<application> instance_;
    std::shared_ptr<logging::logger> logger_;
    spiration::widget* widget_;
    std::shared_ptr<spiration::window> window_;
};

}
