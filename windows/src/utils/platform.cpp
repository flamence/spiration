/**
 * @file platform.cpp
 * @brief 平台实现。
 * @author clk
 */

#include <filesystem>
#include <utils/platform.h>
#include <utils/console.h>

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

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
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(L"ntdll"), "RtlGetVersion")));
    if (RtlGetVersion) {
        RTL_OSVERSIONINFOW vi = {};
        vi.dwOSVersionInfoSize = sizeof(vi);
        if (RtlGetVersion(&vi) == 0) {
            return "Windows " + std::to_string(vi.dwMajorVersion) + "." +
                   std::to_string(vi.dwMinorVersion) + " (Build " +
                   std::to_string(vi.dwBuildNumber) + ")";
        }
    }
    return "Windows";
}

std::string platform::os_version() {
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(L"ntdll"), "RtlGetVersion")));
    if (RtlGetVersion) {
        RTL_OSVERSIONINFOW vi = {};
        vi.dwOSVersionInfoSize = sizeof(vi);
        if (RtlGetVersion(&vi) == 0) {
            return std::to_string(vi.dwBuildNumber);
        }
    }
    return "0";
}

std::string platform::architecture() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        case PROCESSOR_ARCHITECTURE_ARM:   return "arm";
        default: return "unknown";
    }
}

std::filesystem::path platform::data_directory() {
    wchar_t* appdata = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&appdata, &len, L"APPDATA") == 0 && appdata) {
        std::string result = wide_to_utf8(appdata);
        free(appdata);
        return std::filesystem::path(result) / "spiration";
    }
    if (appdata) free(appdata);
    return temp_directory() / "data";
}

std::string platform::system_locale() {
    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH];
    int ret = GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH);
    if (ret > 0) {
        std::string result = wide_to_utf8(locale_name);
        std::string simplified = result.substr(0, 2);
        auto last_dash = result.find_last_of('-');
        if (last_dash != std::string::npos && last_dash > 2) {
            std::string region = result.substr(last_dash + 1);
            simplified += "-" + region;
        } else if (last_dash == 2) {
            simplified = result;
        }
        return simplified;
    }
    return "zh-CN";
}

void platform::open_url(const std::string& url) {
    if (url.empty()) return;
    std::wstring wurl = utf8_to_wide(url);
    HINSTANCE hr = ShellExecuteW(nullptr, L"open", wurl.c_str(),
                                 nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hr) <= 32)
        console::warning("platform", "open_url failed: %s", url.c_str());
}

} // namespace spiration
