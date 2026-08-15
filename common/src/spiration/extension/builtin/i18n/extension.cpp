#include <spiration/extension/builtin/i18n/extension.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/extension/extension_api.h>
#include <filesystem>
#include <spiration/utils/platform.h>
#include <string>

namespace spiration {
namespace i18n {

bool extension::initialize() {
    std::filesystem::path langDir = spiration::platform::resources_directory() / "lang";
    auto& i18n = i18n_manager::get();
    i18n.load("zh-CN", langDir / "zh-CN.properties");
    std::string sysLocale = spiration::platform::system_locale();
    std::filesystem::path langPath = langDir / (std::format("{}.properties", sysLocale));
    i18n.load(sysLocale, langPath);
    i18n.set_locale(sysLocale);

    register_service("i18n", &i18n);

    return true;
}

void extension::shutdown() {
    api->log_info("shutdown");
}

} // namespace i18n
} // namespace spiration
