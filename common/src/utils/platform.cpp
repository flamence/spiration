/**
 * @file platform.cpp
 * @brief 跨平台抽象 API 实现。
 * @author clk
 */

#include <utils/platform.h>
#include <utils/console.h>

#include <cstring>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#elif defined(__OHOS__)
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <cstdlib>
#include <cerrno>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <cstdlib>
#endif

namespace spiration {

os_type platform::current_os() {
#ifdef _WIN32
    return os_type::windows;
#elif defined(__APPLE__)
    return os_type::macos;
#elif defined(__linux__)
    return os_type::linux;
#elif defined(__OHOS__)
    return os_type::ohos;
#else
    return os_type::unknown;
#endif
}

std::string platform::os_name() {
#ifdef _WIN32
    typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(L"ntdll"), "RtlGetVersion")));
    if (RtlGetVersion) {
        RTL_OSVERSIONINFOW vi = {};
        vi.dwOSVersionInfoSize = sizeof(vi);
        if (RtlGetVersion(&vi) == 0) {
            return "Windows " + std::to_string(vi.dwMajorVersion) +
                   "." + std::to_string(vi.dwMinorVersion) +
                   " (Build " + std::to_string(vi.dwBuildNumber) + ")";
        }
    }
    return "Windows";
#elif defined(__APPLE__)
    struct utsname uts;
    uname(&uts);
    return std::string(uts.sysname) + " " + uts.release;
#elif defined(__linux__)
    struct utsname uts;
    uname(&uts);
    return std::string(uts.sysname) + " " + uts.release;
#elif defined(__OHOS__)
    struct utsname uts;
    uname(&uts);
    return "OpenHarmony " + std::string(uts.release);
#else
    return "Unknown";
#endif
}

std::string platform::os_version() {
#ifdef _WIN32
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
#elif defined(__OHOS__)
    struct utsname uts;
    uname(&uts);
    return std::string(uts.release);
#else
    struct utsname uts;
    uname(&uts);
    return uts.release;
#endif
}

std::string platform::architecture() {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        case PROCESSOR_ARCHITECTURE_ARM:   return "arm";
        default: return "unknown";
    }
#elif defined(__OHOS__)
    struct utsname uts;
    uname(&uts);
    return uts.machine;
#else
    struct utsname uts;
    uname(&uts);
    return uts.machine;
#endif
}

std::string platform::app_data_dir() {
#ifdef _WIN32
    wchar_t* appdata = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&appdata, &len, L"APPDATA") == 0 && appdata) {
        int size = WideCharToMultiByte(CP_UTF8, 0, appdata, -1,
                                        nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, appdata, -1,
                            &result[0], size, nullptr, nullptr);
        result.pop_back();
        free(appdata);
        return result + "\\spiration";
    }
    free(appdata);
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (home) return std::string(home) + "/Library/Application Support/Spiration";
#elif defined(__OHOS__)
    return "/data/storage/el2/base/haps/entry/files/spiration";
#endif
    return join_path(executable_directory(), "spiration");
}

std::string platform::executable_directory() {
#ifdef _WIN32
    std::vector<wchar_t> buf(MAX_PATH);
    DWORD len = GetModuleFileNameW(nullptr, buf.data(),
                                    static_cast<DWORD>(buf.size()));
    if (len == 0) return ".";
    std::wstring path(buf.data(), len);
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos);
    int size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1,
                                    nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1,
                        &result[0], size, nullptr, nullptr);
    result.pop_back();
    return result;
#elif defined(__OHOS__)
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string path(buf);
        auto pos = path.find_last_of('/');
        if (pos != std::string::npos) path.resize(pos);
        return path;
    }
    const char* home = getenv("HOME");
    if (home) return std::string(home);
    return ".";
#else
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string path(buf);
        auto pos = path.find_last_of('/');
        if (pos != std::string::npos) path.resize(pos);
        return path;
    }
    return ".";
#endif
}

std::string platform::join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;

    char last = a.back();
#ifdef _WIN32
    if (last == '\\' || last == '/') {
        return a + b;
    }
    return a + "\\" + b;
#else
    if (last == '/') {
        return a + b;
    }
    return a + "/" + b;
#endif
}

bool platform::file_exists(const std::string& path) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
    DWORD attr = GetFileAttributesW(wpath.c_str());
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0;
#endif
}

bool platform::create_directory(const std::string& path) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
    return CreateDirectoryW(wpath.c_str(), nullptr) != 0 ||
           GetLastError() == ERROR_ALREADY_EXISTS;
#else
    // 递归创建父目录
    auto pos = path.rfind('/');
    if (pos != std::string::npos && pos > 0) {
        std::string parent = path.substr(0, pos);
        struct stat st;
        if (stat(parent.c_str(), &st) != 0) {
            create_directory(parent);
        }
    }
    if (mkdir(path.c_str(), 0755) == 0) return true;
    return errno == EEXIST;
#endif
}

std::vector<std::string> platform::list_directory(const std::string& path) {
    std::vector<std::string> entries;

#ifdef _WIN32
    std::string search_path = path + "\\*";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, search_path.c_str(), -1,
                                    nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, search_path.c_str(), -1,
                        &wpath[0], wlen);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wpath.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return entries;

    do {
        if (wcscmp(ffd.cFileName, L".") == 0 ||
            wcscmp(ffd.cFileName, L"..") == 0) continue;

        int size = WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1,
                                        nullptr, 0, nullptr, nullptr);
        std::string entry(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1,
                            &entry[0], size, nullptr, nullptr);
        entry.pop_back();
        entries.push_back(entry);
    } while (FindNextFileW(hFind, &ffd) != 0);

    FindClose(hFind);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) return entries;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0) continue;
        entries.push_back(ent->d_name);
    }
    closedir(dir);
#endif

    std::sort(entries.begin(), entries.end());
    return entries;
}

std::string platform::extension_directory() {
    return app_data_dir();
}

std::string platform::system_locale() {
#ifdef _WIN32
    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH];
    int ret = GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH);
    if (ret > 0) {
        int size = WideCharToMultiByte(CP_UTF8, 0, locale_name, -1,
                                        nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, locale_name, -1,
                            &result[0], size, nullptr, nullptr);
        result.pop_back();
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
#elif defined(__APPLE__)
    const char* lang = getenv("LANG");
    if (lang) {
        std::string locale(lang);
        auto dot_pos = locale.find('.');
        if (dot_pos != std::string::npos) locale = locale.substr(0, dot_pos);
        auto underscore = locale.find('_');
        if (underscore != std::string::npos) {
            locale[underscore] = '-';
        }
        return locale;
    }
#elif defined(__OHOS__)
    const char* lang = getenv("LANG");
    if (lang) {
        std::string locale(lang);
        auto dot_pos = locale.find('.');
        if (dot_pos != std::string::npos) locale = locale.substr(0, dot_pos);
        auto underscore = locale.find('_');
        if (underscore != std::string::npos) {
            locale[underscore] = '-';
        }
        return locale;
    }
    const char* sysLang = getenv("PKG_LOCALE");
    if (sysLang) {
        return std::string(sysLang);
    }
#else
    const char* lang = getenv("LANG");
    if (lang) {
        std::string locale(lang);
        auto dot_pos = locale.find('.');
        if (dot_pos != std::string::npos) locale = locale.substr(0, dot_pos);
        auto underscore = locale.find('_');
        if (underscore != std::string::npos) {
            locale[underscore] = '-';
        }
        return locale;
    }
#endif
    return "zh-CN";
}

void platform::open_url(const std::string& url) {
    if (url.empty()) return;
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring wurl(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wurl[0], wlen);
    HINSTANCE hr = ShellExecuteW(nullptr, L"open", wurl.c_str(),
                                 nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hr) <= 32)
        console::warning("platform", "open_url failed: %s", url.c_str());
#elif defined(__APPLE__)
    std::string cmd = "open '";
    for (char c : url) {
        if (c == '\'') cmd += "'\\''";
        else cmd += c;
    }
    cmd += "' >/dev/null 2>&1 &";
    std::system(cmd.c_str());
#elif defined(__linux__) && !defined(__OHOS__)
    std::string cmd = "xdg-open '";
    for (char c : url) {
        if (c == '\'') cmd += "'\\''";
        else cmd += c;
    }
    cmd += "' >/dev/null 2>&1 &";
    std::system(cmd.c_str());
#else
    console::warning("platform", "open_url not supported on this platform: %s", url.c_str());
#endif
}

} // namespace spiration
