#include <spiration/extension/builtin/i18n/extension.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/extension/extension_api.h>
#include <ghc/filesystem.hpp>
#include <spiration/utils/platform.h>
#include <string>

namespace spiration {
namespace i18n {

bool extension::initialize() {
    ghc::filesystem::path language_directory = spiration::platform::resources_directory() / "lang";
    auto& i18n = i18n_manager::get();
    tl::optional<std::string> locale = spiration::platform::system_locale();
    if (!locale.has_value() || !i18n.load(locale.value(), language_directory / fmt::format("{}.properties", locale.value()))) {
        locale = "zh-CN";
        i18n.load(locale.value(), language_directory / "zh-CN.properties");
    }
    i18n.set_locale(locale.value());

    application::instance()->extension()->register_service(id(), "i18n", &i18n);

    return true;
}

void extension::shutdown() {
    api->log_info("shutdown");
}

} // namespace i18n
} // namespace spiration
