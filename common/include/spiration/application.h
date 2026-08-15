/**
 * @file application.h
 * @brief 应用定义。
 * @author clk
 */

#pragma once

#include <spiration/extension/extension_manager.h>
#include <filesystem>
#include <spiration/init_phase.h>
#include <memory>
#include <optional>
#include <string>
#include <spiration/window/window.h>

namespace spiration {

/**
 * @brief 应用类。
 */
class application {
public:
    static inline std::string APPLICATION_NAME = "Spiration"; ///< 应用名称。
    static inline std::string APPLICATION_LOWER_NAME = "spiration"; ///< 应用小写名。
    static inline std::string APPLICATION_PACKAGE = "com.flamence.spiration"; ///< 应用包名。

#ifdef __OHOS__
    /**
     * @brief 绑定应用至窗口。
     * @param window 目标窗口指针。
     */
    void bind_window(std::shared_ptr<spiration::window> window);
#endif

    /**
     * @return 该应用附加的拓展管理器实例。
     */
    spiration::extension_manager* extension() const;
    
    /**
     * @brief 初始化应用。
     * @param phase 指定所执行应用初始化阶段。
     * @note 对于鸿蒙平台，默认禁止了对窗口的创建，原因是其窗口伴随鸿蒙应用的生命周期。
     */
    void initialize(std::optional<init_phase> phase = std::nullopt);
    
    /**
     * @return 应用实例。
     */
    static application* instance();
    
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
    static std::unique_ptr<application> instance_;
    spiration::widget* widget_;
    std::shared_ptr<spiration::window> window_;

    std::filesystem::path data_directory(std::optional<std::string> sub = std::nullopt);
};

}
