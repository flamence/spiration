/**
 * @file napi_bridge.h
 * @brief 原生接口桥接模块定义。
 * @author clk
 */

#pragma once

#include "napi/native_api.h"

#include <string>

namespace spiration {
namespace bridge {

napi_value CreatePlatformNamespace(napi_env env);
napi_value CreateI18nNamespace(napi_env env);
napi_value CreateExtensionNamespace(napi_env env);
napi_value CreateFsNamespace(napi_env env);

napi_value NapiGetSystemLocale(napi_env env, napi_callback_info info);
napi_value NapiGetOsName(napi_env env, napi_callback_info info);
napi_value NapiGetOsVersion(napi_env env, napi_callback_info info);
napi_value NapiGetArchitecture(napi_env env, napi_callback_info info);
napi_value NapiGetAppDataDir(napi_env env, napi_callback_info info);
napi_value NapiGetExecutableDir(napi_env env, napi_callback_info info);

napi_value NapiTr(napi_env env, napi_callback_info info);
napi_value NapiSetLocale(napi_env env, napi_callback_info info);
napi_value NapiGetCurrentLocale(napi_env env, napi_callback_info info);
napi_value NapiLoadTranslation(napi_env env, napi_callback_info info);

napi_value NapiFileExists(napi_env env, napi_callback_info info);
napi_value NapiCreateDirectory(napi_env env, napi_callback_info info);
napi_value NapiListDirectory(napi_env env, napi_callback_info info);
napi_value NapiJoinPath(napi_env env, napi_callback_info info);

napi_value NapiLoadExtensions(napi_env env, napi_callback_info info);
napi_value NapiGetExtensionDir(napi_env env, napi_callback_info info);

} // namespace bridge
} // namespace spiration
