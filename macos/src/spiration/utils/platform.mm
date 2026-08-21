/**
 * @file platform.mm
 * @brief 平台实现。
 * @author clk
 */

#include <cstdlib>
#include <ghc/filesystem.hpp>
#include <spiration/utils/platform.h>
#include <string>
#include <sys/utsname.h>
#include <vector>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

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
    NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
    return std::to_string(version.majorVersion) + "." +
           std::to_string(version.minorVersion) + "." +
           std::to_string(version.patchVersion);
}

std::string platform::architecture() {
    struct utsname uts;
    uname(&uts);
    return uts.machine;
}

ghc::filesystem::path platform::data_directory() {
    NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                                                      NSUserDomainMask, YES);
    if ([paths count] > 0) {
        NSString *appSupportDir = paths[0];
        std::string base = [appSupportDir UTF8String];
        return ghc::filesystem::path(base) / "Spiration";
    }

    const char* home = getenv("HOME");
    if (home) return ghc::filesystem::path(home) / "Library" / "Application Support" / "Spiration";
    return temp_directory() / "data";
}

std::string platform::system_locale() {
    NSString *localeIdentifier = [[NSLocale currentLocale] localeIdentifier];
    if (localeIdentifier) {
        return [localeIdentifier UTF8String];
    }

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
    NSString *urlString = [NSString stringWithUTF8String:url.c_str()];
    NSURL *nsurl = [NSURL URLWithString:urlString];
    if (nsurl) {
        [[NSWorkspace sharedWorkspace] openURL:nsurl];
    }
}

ghc::filesystem::path platform::executable_directory() {
    NSString *executablePath = [[NSBundle mainBundle] executablePath];
    if (!executablePath) {
        return ghc::filesystem::current_path();
    }
    ghc::filesystem::path exe_path([executablePath UTF8String]);
    return exe_path.parent_path();
}

ghc::filesystem::path platform::resources_directory() {
    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    if (resourcePath) {
        return ghc::filesystem::path([resourcePath UTF8String]);
    }
    return executable_directory().parent_path() / "Resources";
}

} // namespace spiration
