/**
 * @file extension_api_impl.cpp
 * @brief 扩展 API 上下文的默认实现。
 * @author clk
 */

#include <extension/extension_api.h>
#include <extension/extension.h>
#include <extension/extension_manager.h>
#include <ui/root.h>
#include <ui/tab_bar.h>
#include <ui/theme_manager.h>
#include <utils/i18n.h>
#include <utils/console.h>
#include <utils/platform.h>
#include <window/window.h>
#include <renderer/renderer.h>

#include <cstdio>
#include <cstdarg>

namespace spiration {

extension_api::extension_api(std::shared_ptr<window> win)
    : window_(std::move(win)) {}

std::shared_ptr<renderer> extension_api::get_renderer() const {
    return nullptr;
}

std::shared_ptr<window> extension_api::get_window() const {
    return window_;
}

std::string extension_api::tr(const std::string& key) const {
    return i18n::tr(key);
}

std::string extension_api::tr(const std::string& key,
                                    const std::vector<std::string>& args) const {
    return i18n::tr(key, args);
}

void extension_api::log_info(const char* fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console::info("[%s] %s", current_ext_id_.c_str(), buf);
}

void extension_api::log_warning(const char* fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console::warning("[%s] %s", current_ext_id_.c_str(), buf);
}

void extension_api::log_error(const char* fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console::error("[%s] %s", current_ext_id_.c_str(), buf);
}

std::string extension_api::app_data_dir() const {
    return platform::app_data_dir();
}

std::string extension_api::extension_data_dir(const std::string& extension_id) const {
    return platform::join_path(platform::extension_directory(), extension_id);
}

void extension_api::request_repaint() const {
    if (window_) {
        window_->request_repaint();
    }
}

void extension_api::add_menu_item(const std::string& menu_name,
                                        const std::string& label,
                                        std::function<void()> callback) {
    if (root_) {
        root_->add_menu_item(menu_name, label, std::move(callback));
    }
}

void extension_api::open_tab(std::unique_ptr<tab> t) {
    if (root_) {
        root_->open_tab(std::move(t));
    }
}

void extension_api::register_theme_profile(const std::string& name) {
    theme_manager::register_profile(name);
}

void extension_api::set_theme_param(const std::string& profile,
                                          const std::string& key,
                                          const color& value) {
    theme_manager::set(profile, key, value);
}

extension* extension_api::get_extension(const std::string& id) const {
    return extension_manager::find_extension(id);
}

std::vector<extension*> extension_api::get_extensions() const {
    return extension_manager::extensions();
}

int extension_api::on_event(const std::string& event,
                             std::function<void(const std::string& data)> callback) const {
    return extension_manager::on_event(event, std::move(callback));
}

void extension_api::off_event(int subscription_id) const {
    extension_manager::off_event(subscription_id);
}

void extension_api::emit_event(const std::string& event, const std::string& data) const {
    extension_manager::emit_event(event, data);
}

} // namespace spiration
