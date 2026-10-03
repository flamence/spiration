/**
 * @file main.cpp
 * @brief 应用入口。
 * @author clk
 */

#include <spiration/application.h>
#include <windows.h>

namespace {

int run(HINSTANCE hInstance, int nCmdShow) {
    (void)hInstance;
    (void)nCmdShow;

    auto instance = spiration::application::instance();
    if (instance->initialize()) {
        instance->loop();
        instance->shutdown();
    }

    return 0;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    return run(hInstance, nCmdShow);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return run(GetModuleHandleW(nullptr), SW_SHOWDEFAULT);
}
