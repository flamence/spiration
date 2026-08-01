/**
 * @file ohos_application.cpp
 * @brief HarmonyOS 平台 application 特化实现。
 * @author clk
 */

#include <ohos_application.h>
#include <extension/builtin/i18n/i18n.h>
#include <ui/theme_manager.h>
#include <utils/console.h>

namespace spiration {

ohos_application* ohos_application::instance_ = nullptr;

ohos_application* ohos_application::instance() {
    if (!instance_) {
        instance_ = new ohos_application();
    }
    return instance_;
}

void ohos_application::initialize_early() {
    // 初始化 i18n
    std::string dataDir = platform::app_data_dir();
    std::string langDir = dataDir + "/lang";
    std::string sysLocale = platform::system_locale();

    i18n_manager::get().load("zh-CN", langDir + "/zh-CN.txt");
    i18n_manager::get().load(sysLocale, langDir + "/" + sysLocale + ".txt");
    i18n_manager::get().set_locale(sysLocale);

    console::info("ohos/application", "i18n initialized, locale: %s", sysLocale.c_str());

    // 设置 HarmonyOS 兼容字体
    // OpenHarmony 系统支持的字体：sans-serif（默认）、serif、monospace
    theme_manager::set_str(theme_manager::UI_FONT, "sans-serif");
    theme_manager::set_str(theme_manager::INPUT_FONT, "sans-serif");
    theme_manager::set_str(theme_manager::EDITOR_FONT, "monospace");
    console::info("ohos/application", "fonts set for HarmonyOS compatibility");

    // 创建扩展管理器（单例，注册内置扩展）
    extension_manager::instance();

    // 加载扩展
    std::string extDir = platform::extension_directory();
    size_t extCount = extension_manager::load_extensions_from(extDir);
    console::info("ohos/application", "loaded %zu extension(s)", extCount);

    // 初始化 early 阶段扩展
    size_t earlyCount = extension_manager::initialize_phase(init_phase::early);
    console::info("ohos/application", "initialized %zu early extension(s)", earlyCount);
}

void ohos_application::set_window(std::shared_ptr<window> window, int32_t width, int32_t height) {
    window_ = window;

    // 创建 root widget
    auto root = std::make_unique<spiration::root>(window_);
    if (width > 0 && height > 0) {
        root->width = static_cast<float>(width);
        root->height = static_cast<float>(height);
        root->layout();
    }
    widget_ = root.get();
    window_->set_widget(std::move(root));
}

void ohos_application::initialize_normal() {
    // 初始化 normal 阶段扩展
    size_t normalCount = extension_manager::initialize_phase(init_phase::normal);
    console::info("ohos/application", "initialized %zu normal extension(s)", normalCount);

    // 显示窗口
    if (window_) {
        window_->show();
    }
}

void ohos_application::tick(float dt_ms) {
    // 驱动动画和渲染
    if (widget_) {
        widget_->tick(dt_ms);
    }
}

void ohos_application::shutdown() {
    console::info("ohos/application", "shutting down...");

    // 关闭扩展管理器（单例）
    extension_manager::shutdown();

    // 清理资源
    widget_ = nullptr;
    window_.reset();
}

} // namespace spiration
