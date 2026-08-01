/**
 * @file application.cpp
 * @brief HarmonyOS 平台 application 包装，转发到 ohos_application。
 * @author clk
 */

#include <application.h>
#include <ohos_application.h>

namespace spiration {

std::unique_ptr<application> application::instance_;

application* application::instance() {
    // 返回 ohos_application 单例的包装
    // 注意：这里我们直接返回 nullptr，因为 HarmonyOS 使用 ohos_application
    // 这个实现只是为了满足链接器
    return nullptr;
}

extension_manager* application::extension() const {
    return ohos_application::instance()->extension();
}

widget* application::widget() const {
    return ohos_application::instance()->get_widget();
}

window* application::window() const {
    return ohos_application::instance()->get_window();
}

void application::initialize() {
    // HarmonyOS 使用 ohos_application::initialize_early 和 initialize_normal
}

void application::loop() {
    // HarmonyOS 使用 onFrameTick 驱动
}

void application::shutdown() {
    ohos_application::instance()->shutdown();
}

std::shared_ptr<window> application::create_window() {
    // HarmonyOS 使用 set_window 设置窗口
    return nullptr;
}

}
