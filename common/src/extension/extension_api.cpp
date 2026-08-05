/**
 * @file extension_api.cpp
 * @brief 拓展 API 上下文。
 * @author clk
 */

#include <extension/extension_api.h>
#include <extension/extension.h>
#include <extension/extension_manager.h>
#include <ui/root.h>
#include <ui/tab_bar.h>
#include <ui/theme_manager.h>
#include <extension/builtin/i18n/i18n.h>
#include <utils/console.h>
#include <utils/platform.h>
#include <window/window.h>
#include <renderer/renderer.h>
#include <application.h>

#include <cstdio>
#include <cstdarg>
#include <application.h>

namespace spiration {

extension_api::extension_api(std::string id)
    : id_(id) {}

std::shared_ptr<renderer> extension_api::get_renderer() const {
    return nullptr;
}

std::string extension_api::tr(const std::string& key) const {
    return i18n_manager::get().tr(key);
}

std::string extension_api::tr(const std::string& key,
                                    const std::vector<std::string>& args) const {
    return i18n_manager::get().tr(key, args);
}

std::string extension_api::id() const {
    return id_;
}

std::string extension_api::simple_id() const {
    size_t pos = id().find_last_of('.');
    if (pos != std::string::npos) {
        return id().substr(pos + 1);
    }
    return id();
}

void extension_api::log_info(const char* fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    (void)n;
    va_end(args);
    std::string tag = "extension/" + simple_id();
    console::info(tag.c_str(), "%s", buf);
}

void extension_api::log_warning(const char* fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::string tag = "extension/" + simple_id();
    console::warning(tag.c_str(), "%s", buf);
}

void extension_api::log_error(const char* fmt, ...) const {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::string tag = "extension/" + simple_id();
    console::error(tag.c_str(), "%s", buf);
}

std::string extension_api::app_data_dir() const {
    return platform::app_data_dir();
}

std::string extension_api::extension_data_dir(const std::string& extension_id) const {
    return platform::join_path(platform::extension_directory(), extension_id);
}

std::string extension_api::extension_dir() const {
    return extension_manager::extension_directory(id_);
}

void extension_api::request_repaint() const {
    auto window = spiration::application::instance()->window();
    if (window) {
        window->request_repaint();
    }
}

void extension_api::add_menu_item(const std::string& menu_name,
                                        const std::string& label,
                                        std::function<void()> callback) {
    if (auto widget = dynamic_cast<spiration::root*>(spiration::application::instance()->widget())) {
        widget->add_menu_item(menu_name, label, std::move(callback));
    }
}

void extension_api::open_tab(std::unique_ptr<tab> t) {
    if (spiration::root* widget = dynamic_cast<spiration::root*>(spiration::application::instance()->widget())) {
        widget->open_tab(std::move(t));
    }
}

bool extension_api::activate_tab(tab* t) {
    if (!t) return false;
    auto* root = dynamic_cast<spiration::root*>(spiration::application::instance()->widget());
    if (!root) return false;
    auto* tb = root->get_tab_bar();
    if (!tb) return false;
    for (int i = 0; i < tb->tab_count(); ++i) {
        if (tb->get_tab(i) == t) {
            tb->activate_tab(i);
            return true;
        }
    }
    return false;
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
