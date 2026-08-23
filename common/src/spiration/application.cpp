/**
 * @file application.cpp
 * @brief 应用实现。
 * @author clk
 */

#include "spiration/logging/level.h"
#include <spiration/application.h>
#include <spiration/utils/dispatcher.h>
#include <spiration/logging/console_sink.h>
#include <spiration/logging/file_sink.h>
#include <spiration/utils/crash_log.h>
#include <spiration/utils/platform.h>
#include <spiration/ui/root.h>

namespace spiration {

const std::string application::APPLICATION_NAME = "Spiration";
const std::string application::APPLICATION_LOWER_NAME = "spiration";
const std::string application::APPLICATION_PACKAGE = "com.flamence.spiration";

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
    params.width = 1080;
    params.height = 768;
#ifdef __APPLE__
    params.decorated = true;
#else
    params.decorated = false;
#endif
    params.visible = false;
    return spiration::window::create(params);
}
#endif

ghc::filesystem::path make_log_path() {
    std::time_t now = std::time(nullptr);
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d-%H-%M-%S", &tmv);
    return application::instance()->data_directory("logs") / fmt::format("{}.log", ts);
}

}

ghc::filesystem::path application::data_directory(tl::optional<std::string> sub) {
    ghc::filesystem::path base = spiration::platform::data_directory() / APPLICATION_PACKAGE;
    if (sub != tl::nullopt) {
        base = base / *sub;
    }
    ghc::filesystem::create_directories(base);
    return base;
}

std::shared_ptr<spiration::extension_manager> application::extension() const {
    return extension_manager_;
}

task_dispatcher* application::dispatcher() const {
    return dispatcher_.get();
}

application::~application() = default;

bool application::initialize(tl::optional<init_phase> phase) {
    if (phase == tl::nullopt) {
        return initialize(init_phase::early) && initialize(init_phase::normal) && initialize(init_phase::late);
    } else if (phase == init_phase::early) {
        auto file = make_log_path();
        spiration::crash_log::install(file);
        logger_ = std::make_shared<logging::logger>();
        logger_->level = logging::level::trace;
        logger_->add_sink(std::make_unique<logging::console_sink>());
        logger_->add_sink(std::make_unique<logging::file_sink>(file));
        extension_manager_ = std::make_shared<extension_manager>(*this);
        dispatcher_ = std::make_unique<task_dispatcher>();
        extension_manager_->load_from(platform::data_directory());
        extension_manager_->initialize(init_phase::early);
        return true;
    } else if (phase == init_phase::normal) {
#if !defined(__OHOS__)
        window_ = std::move(create_window());
        if (!window_) {
            logger()->log({logging::level::error, "failed to create window", "main"});
            return false;
        }
#endif
#ifdef __APPLE__
        auto widget = std::make_unique<spiration::root>(window_, false);
#else
        auto widget = std::make_unique<spiration::root>(window_);
#endif
        widget_ = widget.get();
        window_->set_widget(std::move(widget));
        extension_manager_->initialize(init_phase::normal);
#if !defined(__OHOS__)
        window_->show();
#endif
        return true;
    } else if (phase == init_phase::late) {
        extension_manager_->initialize(init_phase::late);
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

std::shared_ptr<logging::logger> application::logger() const {
    return logger_;
}

void application::loop() {
    while (window_ && !window_->should_close()) {
        window_->loop();
    }
}

void application::shutdown() {
    window_->close();
    extension_manager_->shutdown();
    if (dispatcher_) dispatcher_->shutdown();
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
