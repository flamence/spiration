/**
 * @file console.cpp
 * @brief 控制台实现。
 * @author clk
 */

#include <filesystem>
#include <utils/console.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
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

#ifndef NDEBUG
log_level console::level = log_level::info;
#else
log_level console::level = log_level::debug;
#endif

namespace {
const char* level_prefix(log_level level) {
    switch (level) {
        case log_level::debug:   return "[DEBUG] ";
        case log_level::info:    return "[INFO] ";
        case log_level::warning: return "[WARN] ";
        case log_level::error:   return "[ERROR] ";
    }
    return "";
}

std::mutex g_log_mtx;
std::ofstream g_log_file;
std::filesystem::path g_log_path;
} // namespace

void console::set_log_file(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lk(g_log_mtx);
    if (g_log_file.is_open()) g_log_file.close();
    g_log_path = path;
    if (!path.empty()) {
        g_log_file.open(path, std::ios::app);
    }
}

std::filesystem::path console::log_file_path() {
    std::lock_guard<std::mutex> lk(g_log_mtx);
    return g_log_path;
}

std::filesystem::path console::make_log_path(const std::filesystem::path& dir, const std::string& prefix) {
    std::time_t now = std::time(nullptr);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tmv);
    return dir / std::format("{}-{}.log", prefix, ts);
}

void console::set_level(log_level level) {
    console::level = level;
}

log_level console::get_level() {
    return level;
}

void console::debug(const char* tag, const char* format, ...) {
    if (level > log_level::debug) return;
    va_list args;
    va_start(args, format);
    vprint(log_level::debug, tag, format, args);
    va_end(args);
}

void console::info(const char* tag, const char* format, ...) {
    if (level > log_level::info) return;
    va_list args;
    va_start(args, format);
    vprint(log_level::info, tag, format, args);
    va_end(args);
}

void console::warning(const char* tag, const char* format, ...) {
    if (level > log_level::warning) return;
    va_list args;
    va_start(args, format);
    vprint(log_level::warning, tag, format, args);
    va_end(args);
}

void console::error(const char* tag, const char* format, ...) {
    if (level > log_level::error) return;
    va_list args;
    va_start(args, format);
    vprint(log_level::error, tag, format, args);
    va_end(args);
}

void console::vprint(log_level level, const char* tag, const char* format, va_list args) {
    char buf[4096];
    int n = 0;
    n += std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "%s", level_prefix(level));
    if (tag && *tag) n += std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "[%s] ", tag);
    {
        va_list copy;
        va_copy(copy, args);
        n += std::vsnprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), format, copy);
        va_end(copy);
    }
    if (n < 0) n = 0;
    if (static_cast<size_t>(n) >= sizeof(buf)) n = static_cast<int>(sizeof(buf)) - 1;
    buf[n] = '\0';

#ifdef __OHOS__
    LogLevel ohos_level = LOG_INFO;
    switch (level) {
        case log_level::debug:   ohos_level = LOG_DEBUG; break;
        case log_level::info:    ohos_level = LOG_INFO;  break;
        case log_level::warning: ohos_level = LOG_WARN;  break;
        case log_level::error:   ohos_level = LOG_ERROR; break;
    }
    std::string fmt;
    for (const char* p = format; *p; ++p) {
        if (*p == '%') {
            if (p[1] == '%') {
                fmt += "%%";
                ++p;
            } else {
                fmt += "%{public}";
            }
        } else {
            fmt += *p;
        }
    }
    OH_LOG_VPrint(LOG_APP, ohos_level, 0x0000, tag, fmt.c_str(), args);
#else
    printf("%s\n", buf);
#endif

    {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        if (g_log_file.is_open()) {
            g_log_file << buf << "\n";
            g_log_file.flush();
        }
    }
}

} 
