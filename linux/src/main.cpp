/**
 * @file main.cpp
 * @brief 应用入口。
 * @author clk
 */

#include <application.h>
#include <utils/console.h>
#include <utils/crash_log.h>
#include <utils/platform.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::string log_dir = spiration::platform::join_path(
        spiration::platform::executable_directory(), "logs");
    spiration::platform::create_directory(log_dir);
    std::string log_path = spiration::console::make_log_path(log_dir, "spiration");
    spiration::console::set_log_file(log_path);
    spiration::crash_log::install(log_path);

    auto instance = spiration::application::instance();
    instance->initialize();
    instance->loop();
    instance->shutdown();
    return 0;
}
