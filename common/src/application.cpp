/**
 * @file application.cpp
 * @brief 应用实现。
 * @author clk
 */

#include <init_phase.h>
#include <utils/console.h>
#include <application.h>
#include <extension/builtin/i18n/i18n.h>
#include <filesystem>
#include <optional>
#include <string>
#include <ui/root.h>
#include <utility>
#include <utils/crash_log.h>
#include <utils/platform.h>

namespace spiration {

#ifdef __OHOS__
void application::bind_window(std::shared_ptr<spiration::window> window) {
    window_ = window;
}
#endif

namespace {

#ifndef __OHOS__
std::shared_ptr<spiration::window> create_window() {
    spiration::window_parameters params;
    params.title = application::APPLICATION_NAME;
    params.width = 800;
    params.height = 600;
    params.decorated = false;
    params.visible = false;
    auto window = spiration::window::create(params);
    if (!window) {
        spiration::console::error("window", "expect a window instance.");
    }
    return window;
}
#endif

}

std::filesystem::path application::data_directory(std::optional<std::string> sub) {
    std::filesystem::path base = spiration::platform::data_directory() / APPLICATION_PACKAGE;
    if (sub != std::nullopt) {
        base = base / *sub;
    }
    std::filesystem::create_directories(base);
    return base;
}

spiration::extension_manager* application::extension() const {
    return &spiration::extension_manager::instance();
}

void application::initialize(std::optional<init_phase> phase) {
    if (phase == std::nullopt) {
        initialize(init_phase::early);
        initialize(init_phase::normal);
        initialize(init_phase::late);
    } else if (phase == init_phase::early) {
        auto log_path = console::make_log_path(data_directory("logs"), APPLICATION_LOWER_NAME);
        spiration::console::set_log_file(log_path);
        spiration::crash_log::install(log_path);
        spiration::extension_manager::instance();
        spiration::console::info("extension/manager", "loaded %zu extension(s)", extension_manager::load_extensions_from(platform::data_directory()));
        spiration::extension_manager::initialize_phase(init_phase::early);
    } else if (phase == init_phase::normal) {
#ifndef __OHOS__
        window_ = std::move(create_window());
#endif
        auto widget = std::make_unique<spiration::root>(window_);
        widget_ = widget.get();
        window_->set_widget(std::move(widget));
        spiration::extension_manager::initialize_phase(init_phase::normal);
#ifndef __OHOS__
        window_->show();
#endif
    } else if (phase == init_phase::late) {
        spiration::extension_manager::initialize_phase(init_phase::late);
    }
}

std::unique_ptr<application> application::instance_ = nullptr;

application* application::instance() {
    if (!instance_) {
        instance_ = std::make_unique<application>();
    }
    return instance_.get();
}

void application::loop() {
    while (!window_->should_close()) {
        window_->loop();
    }
}

void application::shutdown() {
    spiration::extension_manager::shutdown();
}

void application::tick(float deltatime) {
    if (widget_) {
        widget_->tick(deltatime);
    }
}

spiration::widget* application::widget() const {
    return widget_;
}

spiration::window* application::window() const {
    return window_.get();
}

} // namespace spiration
