/**
 * @file application.cpp
 * @brief 应用实现。
 * @author clk
 */

#include <memory>
#include <spiration/logging/file_sink.h>
#include <spiration/logging/console_sink.h>
#include <spiration/init_phase.h>
#include <spiration/application.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <filesystem>
#include <optional>
#include <string>
#include <spiration/ui/root.h>
#include <utility>
#include <spiration/utils/crash_log.h>
#include <spiration/utils/platform.h>

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
    return spiration::window::create(params);
}
#endif

std::filesystem::path make_log_path() {
    std::time_t now = std::time(nullptr);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d-%H-%M-%S", &tmv);
    return application::instance()->data_directory("logs") / std::format("{}.log", ts);
}

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

bool application::initialize(std::optional<init_phase> phase) {
    if (phase == std::nullopt) {
        return initialize(init_phase::early) && initialize(init_phase::normal) && initialize(init_phase::late);
    } else if (phase == init_phase::early) {
        spiration::extension_manager::instance();
        spiration::extension_manager::load_extensions_from(platform::data_directory());
        spiration::extension_manager::initialize_phase(init_phase::early);
        return true;
    } else if (phase == init_phase::normal) {
#ifndef __OHOS__
        window_ = std::move(create_window());
        if (!window_) {
            logger()->log({ .level = logging::level::error, .message = "failed to create window", .tag = "main" });
            return false;
        }
#endif
        auto widget = std::make_unique<spiration::root>(window_);
        widget_ = widget.get();
        window_->set_widget(std::move(widget));
        spiration::extension_manager::initialize_phase(init_phase::normal);
#ifndef __OHOS__
        window_->show();
#endif
        return true;
    } else if (phase == init_phase::late) {
        spiration::extension_manager::initialize_phase(init_phase::late);
        return true;
    }
    return false;
}

std::unique_ptr<application> application::instance_ = nullptr;

application* application::instance() {
    if (!instance_) {
        instance_ = std::make_unique<application>();
    }
    return instance_.get();
}

std::shared_ptr<logging::logger> application::logger() {
    if (logger_ == nullptr) {
        auto file = make_log_path();
        spiration::crash_log::install(file);
        logger_ = std::make_shared<logging::logger>();
        logger_->add_sink(std::make_unique<logging::console_sink>());
        logger_->add_sink(std::make_unique<logging::file_sink>(file));
    }
    return logger_;
}

void application::loop() {
    window_->loop();
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
