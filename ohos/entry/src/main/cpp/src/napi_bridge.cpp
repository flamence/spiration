/**
 * @file napi_bridge.cpp
 * @brief NAPI 桥接实现。
 * @author clk
 */

#include <napi_bridge.h>

#include <spiration/application.h>

#include <fmt/format.h>
#include <string>

namespace spiration {

void set_ohos_data_dir(const std::string& app_dir, const std::string& module_dir);

namespace bridge {

static napi_value CreateString(napi_env env, const std::string& str) {
    napi_value result;
    napi_create_string_utf8(env, str.c_str(), str.length(), &result);
    return result;
}

static std::string GetStringArg(napi_env env, napi_value value) {
    size_t len = 0;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &len) != napi_ok) return "";
    if (len == 0) return "";
    std::string result(len + 1, '\0');
    napi_get_value_string_utf8(env, value, &result[0], len + 1, &len);
    result.resize(len);
    return result;
}

/**
 * @brief 通过 Context API 注入应用沙箱数据目录。
 */
napi_value NapiSetDataDir(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 1) return CreateString(env, "");

    std::string app_dir = GetStringArg(env, args[0]);
    std::string module_dir = argc >= 2 ? GetStringArg(env, args[1]) : app_dir;
    spiration::set_ohos_data_dir(app_dir, module_dir);
    application::instance()->logger()->log({logging::level::info, fmt::format("data dir injected via Context API: app={} module={}", app_dir, module_dir), "napi"});
    return CreateString(env, app_dir);
}

napi_value CreatePlatformNamespace(napi_env env) {
    napi_value ns;
    napi_create_object(env, &ns);

    napi_property_descriptor props[] = {
        { "setDataDir", nullptr, NapiSetDataDir, nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_define_properties(env, ns, sizeof(props) / sizeof(props[0]), props);
    return ns;
}

} // namespace bridge
} // namespace spiration
