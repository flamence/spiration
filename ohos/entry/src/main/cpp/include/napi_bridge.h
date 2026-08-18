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

napi_value NapiSetDataDir(napi_env env, napi_callback_info info);

napi_value NapiRegisterHttpHandler(napi_env env, napi_callback_info info);
napi_value NapiHttpStreamData(napi_env env, napi_callback_info info);
napi_value NapiHttpStreamEnd(napi_env env, napi_callback_info info);
napi_value NapiHttpRequestDone(napi_env env, napi_callback_info info);

} // namespace bridge
} // namespace spiration
