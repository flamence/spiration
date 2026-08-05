/**
 * @file application.cpp
 * @brief 应用实现。
 * @author clk
 */

#include <application.h>
#include <extension/builtin/i18n/i18n.h>

namespace spiration {

std::unique_ptr<application> application::instance_ = nullptr;

void application::initialize_early() {
    spiration::platform::create_directory(spiration::platform::app_data_dir());
    spiration::extension_manager::instance();
    std::string extDir = spiration::platform::extension_directory();
    spiration::platform::create_directory(extDir);
    size_t extCount = spiration::extension_manager::load_extensions_from(extDir);
    spiration::console::info("extension/manager", "loaded %zu extension(s)", extCount);
    spiration::extension_manager::initialize_phase(init_phase::early);
}

void application::set_window(std::shared_ptr<spiration::window> window,
                             int32_t width, int32_t height) {
    window_ = window;
    auto widget = std::make_unique<spiration::root>(window_);
    if (width > 0 && height > 0) {
        widget->width = static_cast<float>(width);
        widget->height = static_cast<float>(height);
        widget->layout();
    }
    widget_ = widget.get();
    window_->set_widget(std::move(widget));
}

void application::initialize_normal() {
    spiration::extension_manager::initialize_phase(init_phase::normal);
}

void application::initialize() {
    initialize_early();
    set_window(create_window());
    initialize_normal();
    window_->show();
    spiration::console::info("main", "spiration running on %s", spiration::platform::os_name().c_str());
}

std::shared_ptr<spiration::window> application::create_window() {
    spiration::window_params params;
    params.title = i18n_manager::get().tr("window.title");
    params.width = 800;
    params.height = 600;
    params.decorated = false;
    auto window = spiration::window::create(params);
    if (!window) {
        spiration::console::error("window", "expect a window instance.");
    }
    return window;
}

spiration::extension_manager* application::extension() const {
    return &spiration::extension_manager::instance();
}

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

void application::tick(float dt_ms) {
    if (widget_) widget_->tick(dt_ms);
}

void application::shutdown() {
    spiration::extension_manager::shutdown();
}

spiration::widget* application::widget() const {
    return widget_;
}

spiration::window* application::window() const {
    return window_.get();
}

} // namespace spiration