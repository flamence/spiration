/**
 * @file main.cpp
 * @brief 应用入口。
 * @author clk
 */

#include <spiration/application.h>
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    
    auto instance = spiration::application::instance();
    instance->initialize();
    instance->loop();
    instance->shutdown();
    
    return 0;
}
