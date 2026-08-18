/**
 * @file platform.cpp
 * @brief 平台实现。
 * @author clk
 */

#include <ghc/filesystem.hpp>
#include <fmt/format.h>
#include <spiration/application.h>
#include <spiration/utils/platform.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace spiration {

namespace {
std::string g_app_dir;      // 应用级 filesDir（getApplicationContext().filesDir）
std::string g_module_dir;   // 模块级 filesDir（UIAbilityContext.filesDir）
}

void set_ohos_data_dir(const std::string& app_dir, const std::string& module_dir) {
    g_app_dir = app_dir;
    g_module_dir = module_dir;
}

os_type platform::current_os() {
    return os_type::ohos;
}

std::string platform::os_name() {
    struct utsname uts;
    uname(&uts);
    return "OpenHarmony " + std::string(uts.release);
}

std::string platform::os_version() {
    struct utsname uts;
    uname(&uts);
    return uts.release;
}

std::string platform::architecture() {
    struct utsname uts;
    uname(&uts);
    return uts.machine;
}

ghc::filesystem::path platform::temp_directory() {
    if (!g_app_dir.empty()) {
        return ghc::filesystem::path(g_app_dir) / "cache";
    }
    return "/data/storage/el2/base/cache";
}

ghc::filesystem::path platform::data_directory() {
    if (!g_app_dir.empty()) {
        return g_app_dir;
    }
    return temp_directory() / "data";
}

ghc::filesystem::path platform::resources_directory() {
    if (!g_module_dir.empty()) {
        return g_module_dir;
    }
    return temp_directory() / "share";
}

std::string platform::system_locale() {
    const char* lang = getenv("LANG");
    if (lang) {
        std::string locale(lang);
        auto dot_pos = locale.find('.');
        if (dot_pos != std::string::npos) locale = locale.substr(0, dot_pos);
        auto underscore = locale.find('_');
        if (underscore != std::string::npos) {
            locale[underscore] = '-';
        }
        return locale;
    }
    const char* sysLang = getenv("PKG_LOCALE");
    if (sysLang) {
        return std::string(sysLang);
    }
    return "zh-CN";
}

void platform::open_url(const std::string& url) {
    application::instance()->logger()->log({logging::level::warning, fmt::format("open_url not supported on this platform: {}", url), "platform"});
}

} // namespace spiration
