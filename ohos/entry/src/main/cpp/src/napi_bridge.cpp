/**
 * @file napi_bridge.cpp
 * @brief Spiration OHOS NAPI 桥接实现。
 * @author clk
 */

#include "napi_bridge.h"

#include <utils/platform.h>
#include <extension/builtin/i18n/i18n.h>
#include <utils/console.h>
#include <extension/extension_manager.h>
#include <extension/extension_api.h>

#include <string>
#include <vector>

namespace spiration {
namespace bridge {

using spiration::i18n_manager;
using spiration::console;
using spiration::platform;

static std::shared_ptr<extension_api> s_api_context = nullptr;

bool initialize_runtime(const std::string& data_dir) {
    std::string langDir = data_dir + "/lang";
    std::string sysLocale = platform::system_locale();

    i18n_manager::get().load("zh-CN", langDir + "/zh-CN.properties");
    i18n_manager::get().load(sysLocale, langDir + "/" + sysLocale + ".properties");
    i18n_manager::get().set_locale(sysLocale);

    console::info("napi", "Spiration OHOS runtime initialized, locale: %s",
                  sysLocale.c_str());

    s_api_context = nullptr;

    return true;
}

void shutdown_runtime() {
    if (s_api_context) {
        s_api_context.reset();
    }
    console::info("napi", "Spiration OHOS runtime shutdown");
}

static napi_value CreateString(napi_env env, const std::string& str) {
    napi_value result;
    napi_create_string_utf8(env, str.c_str(), str.length(), &result);
    return result;
}

static std::string GetStringArg(napi_env env, napi_value value) {
    size_t len = 0;
    napi_get_value_string_utf8(env, value, nullptr, 0, &len);
    if (len == 0) return "";
    std::string result(len, '\0');
    napi_get_value_string_utf8(env, value, &result[0], len + 1, &len);
    return result;
}

napi_value NapiGetOsName(napi_env env, napi_callback_info info) {
    return CreateString(env, platform::os_name());
}

napi_value NapiGetOsVersion(napi_env env, napi_callback_info info) {
    return CreateString(env, platform::os_version());
}

napi_value NapiGetArchitecture(napi_env env, napi_callback_info info) {
    return CreateString(env, platform::architecture());
}

napi_value NapiGetSystemLocale(napi_env env, napi_callback_info info) {
    return CreateString(env, platform::system_locale());
}

napi_value NapiGetAppDataDir(napi_env env, napi_callback_info info) {
    return CreateString(env, platform::app_data_dir());
}

napi_value NapiGetExecutableDir(napi_env env, napi_callback_info info) {
    return CreateString(env, platform::executable_directory());
}

napi_value NapiTr(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    std::string key = GetStringArg(env, args[0]);
    return CreateString(env, i18n_manager::get().tr(key));
}

napi_value NapiSetLocale(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    std::string locale = GetStringArg(env, args[0]);
    i18n_manager::get().set_locale(locale);
    return CreateString(env, locale);
}

napi_value NapiGetCurrentLocale(napi_env env, napi_callback_info info) {
    return CreateString(env, i18n_manager::get().get_locale());
}

napi_value NapiLoadTranslation(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    std::string locale = GetStringArg(env, args[0]);
    std::string filepath = GetStringArg(env, args[1]);
    bool ok = i18n_manager::get().load(locale, filepath);
    napi_value result;
    napi_get_boolean(env, ok, &result);
    return result;
}

napi_value NapiFileExists(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    std::string path = GetStringArg(env, args[0]);
    bool ok = platform::file_exists(path);
    napi_value result;
    napi_get_boolean(env, ok, &result);
    return result;
}

napi_value NapiCreateDirectory(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    std::string path = GetStringArg(env, args[0]);
    bool ok = platform::create_directory(path);
    napi_value result;
    napi_get_boolean(env, ok, &result);
    return result;
}

napi_value NapiListDirectory(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    std::string path = GetStringArg(env, args[0]);
    auto entries = platform::list_directory(path);

    napi_value result;
    napi_create_array_with_length(env, entries.size(), &result);
    for (size_t i = 0; i < entries.size(); i++) {
        napi_set_element(env, result, i, CreateString(env, entries[i]));
    }
    return result;
}

napi_value NapiJoinPath(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    std::string a = GetStringArg(env, args[0]);
    std::string b = GetStringArg(env, args[1]);
    return CreateString(env, platform::join_path(a, b));
}

napi_value NapiLoadExtensions(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    std::string dir = GetStringArg(env, args[0]);
    size_t count = extension_manager::load_extensions_from(dir);

    napi_value result;
    napi_create_uint32(env, static_cast<uint32_t>(count), &result);
    return result;
}

napi_value NapiGetExtensionDir(napi_env env, napi_callback_info info) {
    return CreateString(env, platform::extension_directory());
}

napi_value CreatePlatformNamespace(napi_env env) {
    napi_value ns;
    napi_create_object(env, &ns);

    napi_property_descriptor props[] = {
        { "getOsName", nullptr, NapiGetOsName, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getOsVersion", nullptr, NapiGetOsVersion, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getArchitecture", nullptr, NapiGetArchitecture, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getSystemLocale", nullptr, NapiGetSystemLocale, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getAppDataDir", nullptr, NapiGetAppDataDir, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getExecutableDir", nullptr, NapiGetExecutableDir, nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_define_properties(env, ns, sizeof(props) / sizeof(props[0]), props);
    return ns;
}

napi_value CreateI18nNamespace(napi_env env) {
    napi_value ns;
    napi_create_object(env, &ns);

    napi_property_descriptor props[] = {
        { "tr", nullptr, NapiTr, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setLocale", nullptr, NapiSetLocale, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getCurrentLocale", nullptr, NapiGetCurrentLocale, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "loadTranslation", nullptr, NapiLoadTranslation, nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_define_properties(env, ns, sizeof(props) / sizeof(props[0]), props);
    return ns;
}

napi_value CreateFsNamespace(napi_env env) {
    napi_value ns;
    napi_create_object(env, &ns);

    napi_property_descriptor props[] = {
        { "fileExists", nullptr, NapiFileExists, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "createDirectory", nullptr, NapiCreateDirectory, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "listDirectory", nullptr, NapiListDirectory, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "joinPath", nullptr, NapiJoinPath, nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_define_properties(env, ns, sizeof(props) / sizeof(props[0]), props);
    return ns;
}

napi_value CreateExtensionNamespace(napi_env env) {
    napi_value ns;
    napi_create_object(env, &ns);

    napi_property_descriptor props[] = {
        { "loadFromDir", nullptr, NapiLoadExtensions, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getExtensionDir", nullptr, NapiGetExtensionDir, nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_define_properties(env, ns, sizeof(props) / sizeof(props[0]), props);
    return ns;
}

} // namespace bridge
} // namespace spiration
