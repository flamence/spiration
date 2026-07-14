#include "napi/native_api.h"
#include "napi_bridge.h"

#include <ohos_renderer.h>
#include <ohos_window.h>
#include <ui/root.h>
#include <ui/theme.h>
#include <utils/platform.h>
#include <utils/console.h>
#include <utils/i18n.h>

#include <native_window/external_window.h>
#include <rawfile/raw_file_manager.h>
#include <rawfile/raw_file.h>
#include <fstream>

static std::shared_ptr<spiration::ohos_window> g_window;
static std::shared_ptr<spiration::ohos_renderer> g_renderer;
static NativeResourceManager* g_resourceMgr = nullptr;

/* NAPI 回调注册辅助宏：声明存储 + invoke + NAPI 函数 */
#define DECL_NAPI_CALLBACK(name) \
    static napi_env g_##name##Env = nullptr; \
    static napi_ref g_##name##Ref = nullptr; \
    static void invoke_##name##_callback() { \
        if (g_##name##Env && g_##name##Ref) { \
            napi_value cb; napi_get_reference_value(g_##name##Env, g_##name##Ref, &cb); \
            napi_value undefined; napi_get_undefined(g_##name##Env, &undefined); \
            napi_call_function(g_##name##Env, undefined, cb, 0, nullptr, nullptr); \
        } \
    } \
    static napi_value NapiRegister##name(napi_env env, napi_callback_info info) { \
        size_t argc = 1; napi_value args[1]; \
        napi_get_cb_info(env, info, &argc, args, nullptr, nullptr); \
        g_##name##Env = env; \
        if (g_##name##Ref) { napi_delete_reference(env, g_##name##Ref); g_##name##Ref = nullptr; } \
        napi_create_reference(env, args[0], 1, &g_##name##Ref); \
        return args[0]; \
    }

DECL_NAPI_CALLBACK(CloseCallback)     /* → terminateSelf */
DECL_NAPI_CALLBACK(MaximizeCallback)  /* → win.maximize() / win.recover() */
DECL_NAPI_CALLBACK(MinimizeCallback)  /* → win.minimize() */
DECL_NAPI_CALLBACK(StartMoveCallback) /* → win.startMoving() */

static napi_value NapiInitNativeWindow(napi_env env, napi_callback_info info);
static napi_value NapiInitResourceManager(napi_env env, napi_callback_info info);
static napi_value NapiOnTouchEvent(napi_env env, napi_callback_info info);
static napi_value NapiOnWindowResize(napi_env env, napi_callback_info info);
static napi_value NapiOnFrameTick(napi_env env, napi_callback_info info);
static napi_value NapiRegisterCloseCallback(napi_env env, napi_callback_info info);
static napi_value NapiRegisterMaximizeCallback(napi_env env, napi_callback_info info);
static napi_value NapiRegisterMinimizeCallback(napi_env env, napi_callback_info info);
static napi_value NapiRegisterStartMoveCallback(napi_env env, napi_callback_info info);

/**
 * @brief 初始化 Spiration 桥接层。
 */
EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    std::string dataDir = spiration::platform::app_data_dir();
    spiration::bridge::initialize_runtime(dataDir);

    napi_value spirationNs;
    napi_create_object(env, &spirationNs);

    napi_property_descriptor namespaces[] = {
        {"platform", nullptr, nullptr, nullptr, nullptr, spiration::bridge::CreatePlatformNamespace(env), napi_static, nullptr},
        {"i18n", nullptr, nullptr, nullptr, nullptr, spiration::bridge::CreateI18nNamespace(env), napi_static, nullptr},
        {"fs", nullptr, nullptr, nullptr, nullptr, spiration::bridge::CreateFsNamespace(env), napi_static, nullptr},
        {"extension", nullptr, nullptr, nullptr, nullptr, spiration::bridge::CreateExtensionNamespace(env), napi_static, nullptr},
    };

    napi_define_properties(env, spirationNs, sizeof(namespaces) / sizeof(namespaces[0]), namespaces);

    napi_property_descriptor desc[] = {
        {"spiration", nullptr, nullptr, nullptr, nullptr, spirationNs, napi_default, nullptr},
    };

    napi_property_descriptor native_funcs[] = {
        {"initNativeWindow", nullptr, NapiInitNativeWindow, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onTouchEvent", nullptr, NapiOnTouchEvent, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onWindowResize", nullptr, NapiOnWindowResize, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onFrameTick", nullptr, NapiOnFrameTick, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"initResourceManager", nullptr, NapiInitResourceManager, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"registerCloseCallback", nullptr, NapiRegisterCloseCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"registerMaximizeCallback", nullptr, NapiRegisterMaximizeCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"registerMinimizeCallback", nullptr, NapiRegisterMinimizeCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"registerStartMoveCallback", nullptr, NapiRegisterStartMoveCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
    };

    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    napi_define_properties(env, exports, sizeof(native_funcs) / sizeof(native_funcs[0]), native_funcs);

    spiration::console::info("Spiration NAPI module initialized");
    return exports;
}
EXTERN_C_END

/**
 * @brief 将 rawfile 内容写出到文件系统后加载到 i18n。
 */
static bool load_lang_from_rawfile(const std::string& locale, const std::string& filename) {
    if (!g_resourceMgr) return false;
    RawFile* raw = OH_ResourceManager_OpenRawFile(g_resourceMgr, ("lang/" + filename).c_str());
    if (!raw) {
        spiration::console::warning("rawfile not found: lang/%s", filename.c_str());
        return false;
    }
    long size = OH_ResourceManager_GetRawFileSize(raw);
    if (size <= 0) {
        OH_ResourceManager_CloseRawFile(raw);
        return false;
    }
    std::string content(static_cast<size_t>(size), '\0');
    long read = OH_ResourceManager_ReadRawFile(raw, &content[0], size);
    OH_ResourceManager_CloseRawFile(raw);
    if (read <= 0) return false;

    std::string tmpDir = spiration::platform::app_data_dir() + "/lang";
    spiration::platform::create_directory(tmpDir);
    std::string tmpFile = tmpDir + "/" + filename;
    std::ofstream out(tmpFile, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(content.data(), content.size());
    out.close();

    return spiration::i18n::load(locale, tmpFile);
}

/**
 * @brief 从 ArkUI 接收 resourceManager，初始化原生资源管理器并加载翻译文件。
 */
static napi_value NapiInitResourceManager(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        napi_value ret;
        napi_get_boolean(env, false, &ret);
        return ret;
    }

    g_resourceMgr = OH_ResourceManager_InitNativeResourceManager(env, args[0]);
    if (!g_resourceMgr) {
        spiration::console::error("Failed to init native resource manager");
        napi_value ret;
        napi_get_boolean(env, false, &ret);
        return ret;
    }

    std::string sysLocale = spiration::platform::system_locale();
    load_lang_from_rawfile("zh-CN", "zh-CN.txt");
    load_lang_from_rawfile("en-US", "en-US.txt");
    if (sysLocale != "zh-CN" && sysLocale != "en-US") {
        load_lang_from_rawfile(sysLocale, sysLocale + ".txt");
    }
    spiration::i18n::set_locale(sysLocale);
    spiration::console::info("ResourceManager initialized, locale=%s", sysLocale.c_str());
    napi_value ret;
    napi_get_boolean(env, true, &ret);
    return ret;
}

/**
 * @brief 从 ArkUI XComponent 的 surface ID 初始化原生渲染器。
 */
static napi_value NapiInitNativeWindow(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    int64_t surface_id = 0;
    napi_get_value_int64(env, args[0], &surface_id);

    double density = 1.0;
    if (argc >= 2) {
        napi_get_value_double(env, args[1], &density);
    }

    OHNativeWindow* native_window = nullptr;
    OH_NativeWindow_CreateNativeWindowFromSurfaceId(
        static_cast<uint64_t>(surface_id), &native_window);
    if (!native_window) {
        spiration::console::error("Failed to get native window from surface id");
        napi_value ret;
        napi_get_boolean(env, false, &ret);
        return ret;
    }

    g_renderer = std::make_shared<spiration::ohos_renderer>();
    if (!g_renderer->initialize(native_window)) {
        spiration::console::error("Failed to initialize OHOS renderer");
        napi_value ret;
        napi_get_boolean(env, false, &ret);
        return ret;
    }

    uint32_t actual_w = 0, actual_h = 0;
    g_renderer->get_viewport_size(actual_w, actual_h);
    if (actual_w == 0 || actual_h == 0) {
        actual_w = 1200; actual_h = 800;
    }

    float dpi = static_cast<float>(density > 0.0 ? density : 1.0);
    g_renderer->set_density(dpi);
    uint32_t logical_w = static_cast<uint32_t>(actual_w / dpi);
    uint32_t logical_h = static_cast<uint32_t>(actual_h / dpi);

    spiration::console::info("DPI=%.1f, physical=%dx%d, logical=%dx%d",
        dpi, actual_w, actual_h, logical_w, logical_h);

    g_renderer->set_logical_size(logical_w, logical_h);

    spiration::window_params params;
    params.title = "Spiration";
    params.width = static_cast<int32_t>(logical_w);
    params.height = static_cast<int32_t>(logical_h);

    g_window = std::make_shared<spiration::ohos_window>();
    g_window->initialize(params);
    g_window->set_renderer(g_renderer);

    auto root_ptr = std::make_unique<spiration::root>(g_window);
    root_ptr->width = static_cast<float>(logical_w);
    root_ptr->height = static_cast<float>(logical_h);
    root_ptr->layout();

    g_window->set_on_close([](void*) { invoke_CloseCallback_callback(); });
    g_window->set_on_maximize([](void*) { invoke_MaximizeCallback_callback(); });
    g_window->set_on_minimize([](void*) { invoke_MinimizeCallback_callback(); });
    g_window->set_on_start_move([](void*) { invoke_StartMoveCallback_callback(); });

    g_window->set_widget(std::move(root_ptr));

    if (g_renderer) {
        g_window->render(g_renderer);
    }

    spiration::console::info("Spiration native window initialized");

    napi_value ret;
    napi_get_boolean(env, true, &ret);
    return ret;
}

/**
 * @brief 从 ArkUI 接收点击坐标并转发到 widget 树。
 */
static napi_value NapiOnTouchEvent(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    double x = 0, y = 0, action = 0;
    napi_get_value_double(env, args[0], &x);
    napi_get_value_double(env, args[1], &y);
    napi_get_value_double(env, args[2], &action);

    if (g_window) {
        g_window->on_touch_event(static_cast<float>(x), static_cast<float>(y),
                                  static_cast<int>(action));
        if (g_renderer) g_window->render(g_renderer);
    }

    napi_value ret;
    napi_get_undefined(env, &ret);
    return ret;
}

/**
 * @brief 每帧由 ArkUI setInterval 调用，驱动 widget 树动画和渲染。
 * @param dt_ms 距离上一帧的毫秒数
 */
static napi_value NapiOnFrameTick(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    double dt_ms = 16.0;
    if (argc >= 1) napi_get_value_double(env, args[0], &dt_ms);

    if (g_window && g_renderer) {
        g_renderer->check_resize();

        uint32_t vp_w = 0, vp_h = 0;
        g_renderer->get_viewport_size(vp_w, vp_h);
        int32_t cur_w = 0, cur_h = 0;
        g_window->get_size(cur_w, cur_h);
        if (static_cast<int32_t>(vp_w) != cur_w || static_cast<int32_t>(vp_h) != cur_h) {
            g_window->set_size(static_cast<int32_t>(vp_w),
                               static_cast<int32_t>(vp_h));
            g_window->resize_widget(static_cast<int32_t>(vp_w),
                                    static_cast<int32_t>(vp_h));
        }

        g_window->tick(static_cast<float>(dt_ms));
        g_window->render(g_renderer);
    }

    napi_value ret;
    napi_get_undefined(env, &ret);
    return ret;
}

/**
 * @brief 接收 ArkUI 窗口大小变化事件，更新渲染器和 widget 树。
 */
static napi_value NapiOnWindowResize(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    double width = 0, height = 0, density = 1.0;
    napi_get_value_double(env, args[0], &width);
    napi_get_value_double(env, args[1], &height);
    if (argc >= 3) {
        napi_get_value_double(env, args[2], &density);
    }

    if (g_renderer && g_window) {
        uint32_t px_w = static_cast<uint32_t>(width);
        uint32_t px_h = static_cast<uint32_t>(height);
        uint32_t vp_w = static_cast<uint32_t>(width / density);
        uint32_t vp_h = static_cast<uint32_t>(height / density);

        g_renderer->set_density(static_cast<float>(density));
        g_renderer->resize(px_w, px_h);
        g_renderer->set_logical_size(vp_w, vp_h);

        g_window->set_size(static_cast<int32_t>(vp_w),
                           static_cast<int32_t>(vp_h));
        g_window->resize_widget(static_cast<int32_t>(vp_w),
                                static_cast<int32_t>(vp_h));

        g_window->render(g_renderer);
    }

    napi_value ret;
    napi_get_undefined(env, &ret);
    return ret;
}

static napi_module spirationModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&spirationModule);
}
