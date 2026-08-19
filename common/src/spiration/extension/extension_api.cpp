/**
 * @file extension_api.cpp
 * @brief 拓展 API 上下文。
 * @author clk
 */

#include <spiration/extension/extension_api.h>
#include <spiration/extension/extension.hpp>
#include <spiration/extension/extension_manager.h>
#include <ghc/filesystem.hpp>
#include <spiration/ui/root.h>
#include <spiration/ui/tab_bar.h>
#include <spiration/ui/theme_manager.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/utils/platform.h>
#include <spiration/window/window.h>
#include <spiration/renderer/renderer.h>
#include <spiration/application.h>

#include <fmt/format.h>
#include <tl/optional.hpp>

namespace spiration {

extension_api::extension_api(std::string id)
    : id_(id) {}

std::shared_ptr<renderer> extension_api::get_renderer() const {
    auto* w = spiration::application::instance()->window();
    return w ? w->get_renderer() : nullptr;
}

std::shared_ptr<window> extension_api::get_window() const {
    auto* w = spiration::application::instance()->window();
    if (!w) return nullptr;
    return std::shared_ptr<window>(w, [](window*) {});
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

void extension_api::log_info(const std::string& message, tl::optional<std::string> tag) const {
    application::instance()->logger()->log({logging::level::info, message, fmt::format("extension/{}{}", simple_id(), tag ? fmt::format("/{}", *tag) : "")});
}

void extension_api::log_warning(const std::string& message, tl::optional<std::string> tag) const {
    application::instance()->logger()->log({logging::level::warning, message, fmt::format("extension/{}{}", simple_id(), tag ? fmt::format("/{}", *tag) : "")});
}

void extension_api::log_error(const std::string& message, tl::optional<std::string> tag) const {
    application::instance()->logger()->log({logging::level::error, message, fmt::format("extension/{}{}", simple_id(), tag ? fmt::format("/{}", *tag) : "")});
}

ghc::filesystem::path extension_api::extension_directory() const {
    return application::instance()->extension()->extension_directory(id_);
}

void extension_api::register_service(const std::string& name, void* svc) const {
    application::instance()->extension()->register_service(id_, name, svc);
}

void* extension_api::get_service(const std::string& extension_id,
                                 const std::string& name) const {
    return application::instance()->extension()->get_service(extension_id, name);
}

void extension_api::load_translations(const ghc::filesystem::path& filepath) const {
    auto& i18n = i18n_manager::get();
    std::string locale = i18n.get_locale();
    if (locale.empty()) locale = "zh-CN";
    if (i18n.load(locale, filepath)) {
        application::instance()->logger()->log(
            {logging::level::info,
             fmt::format("loaded translations from {}", filepath.string()),
             fmt::format("extension/{}/i18n", simple_id())});
    }
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
    return application::instance()->extension()->find_extension(id);
}

std::vector<extension*> extension_api::get_extensions() const {
    return application::instance()->extension()->extensions();
}

int extension_api::on_event(const std::string& event,
                             std::function<void(const std::string& data)> callback) const {
    return application::instance()->extension()->on_event(event, std::move(callback));
}

void extension_api::off_event(int subscription_id) const {
    application::instance()->extension()->off_event(subscription_id);
}

void extension_api::emit_event(const std::string& event, const std::string& data) const {
    application::instance()->extension()->emit_event(event, data);
}

} // namespace spiration
