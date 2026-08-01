/**
 * @file main.cpp
 * @brief 应用入口。
 * @author clk
 */

#include <application.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    auto instance = spiration::application::instance();
    instance->initialize();
    instance->loop();
    instance->shutdown();
    return 0;
}
