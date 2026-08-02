/**
 * @file platform.cpp
 * @brief 平台实现。
 * @author clk
 */

#include <utils/platform.h>
#include <utils/console.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace spiration {

namespace {
std::string g_data_dir;
}

void set_ohos_data_dir(const std::string& dir) {
    g_data_dir = dir;
}

os_type platform::current_os() {
    return os_type::ohos;
}

std::string platform::os_name() {
    struct utsname uts;
    uname(&uts);
    return "OpenHarmony " + std::string(uts.release);
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

std::string platform::app_data_dir() {
    if (!g_data_dir.empty()) {
        return join_path(g_data_dir, "spiration");
    }
    return "/data/storage/el2/base/haps/entry/files/spiration";
}

std::string platform::executable_directory() {
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
}

std::string platform::join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '/') return a + b;
    return a + "/" + b;
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

std::string platform::extension_directory() {
    return app_data_dir();
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
    const char* sysLang = getenv("PKG_LOCALE");
    if (sysLang) {
        return std::string(sysLang);
    }
    return "zh-CN";
}

void platform::open_url(const std::string& url) {
    console::warning("platform", "open_url not supported on this platform: %s",
                     url.c_str());
}

} // namespace spiration
