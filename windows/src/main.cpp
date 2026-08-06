/**
 * @file main.cpp
 * @brief 应用入口。
 * @author clk
 */

#include <application.h>
#include <utils/console.h>
#include <utils/crash_log.h>
#include <utils/platform.h>
#include <windows.h>

#include <cstring>
#include <string>

namespace {

std::string executable_directory() {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return ".";
    char* slash = std::strrchr(path, '\\');
    if (slash) *slash = '\0';
    return std::string(path);
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    SetProcessDPIAware();
    std::string log_dir = spiration::platform::join_path(executable_directory(), "logs");
    spiration::platform::create_directory(log_dir);
    spiration::console::set_log_file(
        spiration::console::make_log_path(log_dir, "spiration"));
    spiration::crash_log::install(spiration::console::log_file_path());

    auto instance = spiration::application::instance();
    instance->initialize();
    instance->loop();
    instance->shutdown();
    
    return 0;
}