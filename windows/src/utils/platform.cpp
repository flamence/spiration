/**
 * @file platform.cpp
 * @brief 平台实现。
 * @author clk
 */

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

std::string platform::app_data_dir() {
    wchar_t* appdata = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&appdata, &len, L"APPDATA") == 0 && appdata) {
        std::string result = wide_to_utf8(appdata);
        free(appdata);
        return result + "\\spiration";
    }
    if (appdata) free(appdata);
    return join_path(executable_directory(), "spiration");
}

std::string platform::executable_directory() {
    std::vector<wchar_t> buf(MAX_PATH);
    DWORD len = GetModuleFileNameW(nullptr, buf.data(),
                                   static_cast<DWORD>(buf.size()));
    if (len == 0) return ".";
    std::wstring path(buf.data(), len);
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos);
    return wide_to_utf8(path);
}

std::string platform::join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '\\' || last == '/') return a + b;
    return a + "\\" + b;
}

bool platform::file_exists(const std::string& path) {
    DWORD attr = GetFileAttributesW(utf8_to_wide(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES;
}

bool platform::create_directory(const std::string& path) {
    if (path.empty()) return false;
    if (file_exists(path)) return true;
    std::string parent = path;
    while (!parent.empty() && (parent.back() == '\\' || parent.back() == '/'))
        parent.pop_back();
    auto slash = parent.find_last_of("\\/");
    if (slash != std::string::npos && slash > 0) {
        std::string p = parent.substr(0, slash);
        if (!p.empty() && !file_exists(p)) create_directory(p);
    }
    std::wstring wpath = utf8_to_wide(path);
    return CreateDirectoryW(wpath.c_str(), nullptr) != 0 ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

std::vector<std::string> platform::list_directory(const std::string& path) {
    std::vector<std::string> entries;
    std::wstring search = utf8_to_wide(path) + L"\\*";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return entries;
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 ||
            wcscmp(ffd.cFileName, L"..") == 0) continue;
        entries.push_back(wide_to_utf8(ffd.cFileName));
    } while (FindNextFileW(hFind, &ffd) != 0);
    FindClose(hFind);
    std::sort(entries.begin(), entries.end());
    return entries;
}

std::string platform::extension_directory() {
    return app_data_dir();
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
