/**
 * @file console.cpp
 * @brief 统一控制台输出与日志管理工具实现。
 * @author clk
 */

#include <utils/console.h>
#include <cstdio>
#include <cstdarg>
#ifdef __OHOS__
#include <hilog/log.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
    static bool ensure_utf8_console() {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        return true;
    }
    static bool s_utf8_console = ensure_utf8_console();
}
#endif

namespace spiration {

log_level console::s_level = log_level::info;

void console::set_level(log_level level) {
    s_level = level;
}

log_level console::get_level() {
    return s_level;
}

void console::debug(const char* format, ...) {
#ifndef NDEBUG
    if (s_level > log_level::debug) return;
    va_list args;
    va_start(args, format);
    vprint("[DEBUG] ", format, args);
    va_end(args);
#endif
}

void console::info(const char* format, ...) {
    if (s_level > log_level::info) return;
    va_list args;
    va_start(args, format);
    vprint("[INFO]  ", format, args);
    va_end(args);
}

void console::warning(const char* format, ...) {
    if (s_level > log_level::warning) return;
    va_list args;
    va_start(args, format);
    vprint("[WARN]  ", format, args);
    va_end(args);
}

void console::error(const char* format, ...) {
    if (s_level > log_level::error) return;
    va_list args;
    va_start(args, format);
    vprint("[ERROR] ", format, args);
    va_end(args);
}

void console::vprint(const char* prefix, const char* format, va_list args) {
#ifdef __OHOS__
    char buf[1024];
    vsnprintf(buf, sizeof(buf), format, args);
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0000, "Spiration", "%{public}s%{public}s",
                 prefix, buf);
#else
    printf("%s", prefix);
    vprintf(format, args);
    printf("\n");
#endif
}

} 
