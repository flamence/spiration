/**
 * @file platform.cpp
 * @brief 平台实现。
 * @author clk
 */

#include "spiration/application.h"
#include <format>
#include <string>
#include <spiration/utils/platform.h>

#include <windows.h>
#include <shellapi.h>

#include <cstdlib>

namespace spiration {

namespace {

std::wstring utf8_to_wide(const std::string& s) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(wlen) - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], wlen);
    return w;
}

std::string wide_to_utf8(const std::wstring& w) {
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0,
                                   nullptr, nullptr);
    std::string result(static_cast<size_t>(size) - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &result[0], size,
                        nullptr, nullptr);
    return result;
}

} // namespace

os_type platform::current_os() {
    return os_type::windows;
}

std::string platform::os_name() {
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll")) {
        using RtlGetVersionPtr = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);

        if (auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
                GetProcAddress(ntdll, "RtlGetVersion"))) {

            RTL_OSVERSIONINFOW vi{ .dwOSVersionInfoSize = sizeof(vi) };

            if (RtlGetVersion(&vi) == 0) {
                return std::format("Windows {}.{} (Build {})",
                                   vi.dwMajorVersion,
                                   vi.dwMinorVersion,
                                   vi.dwBuildNumber);
            }
        }
    }
    return "Windows";
}

std::string platform::os_version() {
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll")) {
        using RtlGetVersionPtr = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);

        if (auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
                GetProcAddress(ntdll, "RtlGetVersion"))) {

            RTL_OSVERSIONINFOW vi{ .dwOSVersionInfoSize = sizeof(vi) };

            if (RtlGetVersion(&vi) == 0) {
                return std::to_string(vi.dwBuildNumber);
            }
        }
    }
    return "0";
}

std::string platform::architecture() {
    static const std::string arch = [] {
        SYSTEM_INFO si{};
        GetNativeSystemInfo(&si);

        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
            case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
            case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
            case PROCESSOR_ARCHITECTURE_ARM:   return "arm";
            default:                           return "unknown";
        }
    }();

    return arch;
}

std::filesystem::path platform::data_directory() {
    wchar_t* raw = nullptr;
    size_t len = 0;

    if (_wdupenv_s(&raw, &len, L"APPDATA") == 0 && raw) {
        std::unique_ptr<wchar_t, decltype(&free)> appdata(raw, &free);
        return std::filesystem::path(appdata.get()) / "spiration";
    }

    return temp_directory() / "data";
}

std::filesystem::path platform::executable_directory() {
    std::wstring buffer;
    DWORD length = 0;
    do {
        buffer.resize(buffer.size() + MAX_PATH);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    } while (length == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    if (length == 0) {
        return temp_directory() / "bin";
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path platform::resources_directory() {
    return executable_directory().parent_path() / "share";
}

std::string platform::system_locale() {
    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) <= 0)
        return "zh-CN";

    std::wstring wide_locale(locale_name);
    const size_t last_dash = wide_locale.find_last_of(L'-');

    std::wstring simplified;
    if (last_dash == std::wstring::npos) {
        simplified = wide_locale.substr(0, 2);
    } else if (last_dash == 2) {
        simplified = wide_locale;
    } else {
        simplified = wide_locale.substr(0, 2) + L"-" + wide_locale.substr(last_dash + 1);
    }

    const int utf8_size = WideCharToMultiByte(CP_UTF8, 0, simplified.c_str(), -1,
                                              nullptr, 0, nullptr, nullptr);
    if (utf8_size <= 0)
        return "zh-CN";

    std::string result(static_cast<size_t>(utf8_size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, simplified.c_str(), -1,
                        result.data(), utf8_size, nullptr, nullptr);
    result.resize(static_cast<size_t>(utf8_size) - 1);
    return result;
}

void platform::open_url(const std::string& url) {
    if (url.empty())
        return;

    const int wide_size = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1,
                                              nullptr, 0);
    if (wide_size <= 0) {
        application::instance()->logger()->log({ .level = logging::level::warning, .message = std::format("open_url failed: {}", url), .tag = "platform" });
        return;
    }

    std::wstring wurl(static_cast<size_t>(wide_size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1,
                        wurl.data(), wide_size);
    wurl.resize(static_cast<size_t>(wide_size) - 1);   // 移除末尾空字符

    const HINSTANCE hr = ShellExecuteW(nullptr, L"open", wurl.c_str(),
                                       nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hr) <= 32) {
        application::instance()->logger()->log({ .level = logging::level::warning, .message = "failed to create window", .tag = "main" });
    }
}

} // namespace spiration
