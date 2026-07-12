/**
 * @file extension_api_impl.cpp
 * @brief 扩展 API 上下文的默认实现。
 * @author clk
 */

#include <extension/extension_api_impl.h>
#include <ui/root.h>
#include <ui/tab_bar.h>
#include <ui/theme.h>
#include <utils/i18n.h>
#include <utils/console.h>
#include <utils/platform.h>
#include <window/window.h>
#include <renderer/renderer.h>

namespace spiration {

extension_api_impl::extension_api_impl(std::shared_ptr<window> win)
    : window_(std::move(win)) {}

std::shared_ptr<renderer> extension_api_impl::get_renderer() const {
    return nullptr;
}

std::shared_ptr<window> extension_api_impl::get_window() const {
    return window_;
}

std::string extension_api_impl::tr(const std::string& key) const {
    return i18n::tr(key);
}

std::string extension_api_impl::tr(const std::string& key,
                                    const std::vector<std::string>& args) const {
    return i18n::tr(key, args);
}

void extension_api_impl::log_info(const std::string& message) const {
    console::info("%s", message.c_str());
}

void extension_api_impl::log_warning(const std::string& message) const {
    console::warning("%s", message.c_str());
}

void extension_api_impl::log_error(const std::string& message) const {
    console::error("%s", message.c_str());
}

std::string extension_api_impl::app_data_dir() const {
    return platform::app_data_dir();
}

std::string extension_api_impl::extension_data_dir(const std::string& extension_id) const {
    return platform::join_path(platform::extension_directory(), extension_id);
}

void extension_api_impl::request_repaint() const {
    if (window_) {
        window_->request_repaint();
    }
}

void extension_api_impl::add_menu_item(const std::string& menu_name,
                                        const std::string& label,
                                        std::function<void()> callback) {
    if (root_) {
        root_->add_menu_item(menu_name, label, std::move(callback));
    }
}

void extension_api_impl::open_tab(std::unique_ptr<tab> t) {
    if (root_) {
        root_->open_tab(std::move(t));
    }
}

void extension_api_impl::register_theme_profile(const std::string& name) {
    theme::register_profile(name);
}

void extension_api_impl::set_theme_param(const std::string& profile,
                                          const std::string& key,
                                          const color& value) {
    theme::set(profile, key, value);
}

} // namespace spiration
