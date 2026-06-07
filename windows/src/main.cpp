/**
 * @file main.cpp
 * @brief Spiration Windows 平台入口点。
 * @author clk
 */

#include <ui/root.h>
#include <window/window.h>
#include <utils/console.h>
#include <utils/i18n.h>
#include <windows.h>
#include <string>
#include <vector>

/**
 * @brief 获取可执行文件所在目录。
 */
static std::string get_exe_directory() {
    std::vector<wchar_t> buf(MAX_PATH);
    DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (len == 0) return ".";
    std::wstring path(buf.data(), len);
    auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos);
    int size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &result[0], size, nullptr, nullptr);
    result.pop_back();
    return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDPIAware();

    std::string exeDir = get_exe_directory();
    std::string langPath = exeDir + "/lang/zh-CN.txt";
    spiration::i18n::load("zh-CN", langPath);

    spiration::window_params params;
    params.title = "Spiration";
    params.width = 800;
    params.height = 600;
    params.decorated = false;

    auto window = spiration::window::create(params);
    auto root = std::make_unique<spiration::root>(window);
    window->set_widget(std::move(root));
    window->show();
    
    while (!window->should_close()) {
        window->loop();
    }

    return 0;
}