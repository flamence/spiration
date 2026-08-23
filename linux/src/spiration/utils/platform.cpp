/**
 * @file platform.cpp
 * @brief 平台实现。
 * @author clk
 */

#include <ghc/filesystem.hpp>
#include <spiration/utils/platform.h>

#include <unistd.h>
#include <limits.h>
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

os_type platform::current_os() {
    return os_type::linux;
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

ghc::filesystem::path platform::data_directory() {
    const char* home = getenv("HOME");
    if (home) return std::string(home) + "/.config/spiration";
    return temp_directory() / "data";
}

ghc::filesystem::path platform::executable_directory() {
    std::vector<char> buf(PATH_MAX);
    ssize_t len;
    while ((len = ::readlink("/proc/self/exe", buf.data(), buf.size())) == static_cast<ssize_t>(buf.size())) {
        buf.resize(buf.size() * 2);
    }
    if (len != -1) {
        buf[len] = '\0';
        return ghc::filesystem::path(buf.data()).parent_path();
    }
    return ghc::filesystem::current_path();
}

ghc::filesystem::path platform::resources_directory() {
    auto base = executable_directory().parent_path();
    
    for (auto path : {
        base / "share" / "spiration",
        base / "share"
    }) {
        if (ghc::filesystem::exists(path)) {
            return path;
        }
    }

    return ghc::filesystem::current_path();
}

tl::optional<std::string> platform::system_locale() {
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
    return tl::nullopt;
}

void platform::open_url(const std::string& url) {
    if (url.empty()) return;
    std::string cmd = "xdg-open '";
    for (char c : url) {
        if (c == '\'') cmd += "'\\''";
        else cmd += c;
    }
    cmd += "' >/dev/null 2>&1 &";
    std::system(cmd.c_str());
}

} // namespace spiration
