/**
 * @file platform.cpp
 * @brief 平台实现。
 * @author clk
 */

#include <filesystem>
#include <spiration/utils/platform.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <mach-o/dyld.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif

namespace spiration {

os_type platform::current_os() {
    return os_type::macos;
}

std::string platform::os_name() {
    struct utsname uts;
    uname(&uts);
    return std::string(uts.sysname) + " " + uts.release;
}

std::string platform::os_version() {
    struct utsname uts;
    uname(&uts);
    return uts.release;
}

std::string platform::architecture() {
    struct utsname uts;
    uname(&uts);
    return uts.machine;
}

std::filesystem::path platform::data_directory() {
    const char* home = getenv("HOME");
    if (home) return std::string(home) + "/Library/Application Support/Spiration";
    return temp_directory() / "data";
}

bool platform::file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool platform::create_directory(const std::string& path) {
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
}

std::vector<std::string> platform::list_directory(const std::string& path) {
    std::vector<std::string> entries;
    DIR* dir = opendir(path.c_str());
    if (!dir) return entries;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0) continue;
        entries.push_back(ent->d_name);
    }
    closedir(dir);
    std::sort(entries.begin(), entries.end());
    return entries;
}

std::string platform::system_locale() {
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
    return "zh-CN";
}

void platform::open_url(const std::string& url) {
    if (url.empty()) return;
    std::string cmd = "open '";
    for (char c : url) {
        if (c == '\'') cmd += "'\\''";
        else cmd += c;
    }
    cmd += "' >/dev/null 2>&1 &";
    std::system(cmd.c_str());
}

} // namespace spiration
