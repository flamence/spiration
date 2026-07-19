/**
 * @file main.cpp
 * @brief Spiration Windows 平台入口点。
 * @author clk
 */

#include <ui/root.h>
#include <window/window.h>
#include <utils/console.h>
#include <utils/i18n.h>
#include <utils/platform.h>
#include <extension/extension_api.h>
#include <extension/extension_manager.h>
#include <windows.h>
#include <string>
#include <vector>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDPIAware();

    std::string exeDir = spiration::platform::executable_directory();
    std::string langDir = exeDir + "/lang";

    spiration::i18n::load("zh-CN", langDir + "/zh-CN.txt");

    std::string sysLocale = spiration::platform::system_locale();
    std::string langPath = langDir + "/" + sysLocale + ".txt";
    spiration::i18n::load(sysLocale, langPath);
    spiration::i18n::set_locale(sysLocale);

    spiration::console::info("Spiration starting on %s, locale: %s",
                             spiration::platform::os_name().c_str(),
                             sysLocale.c_str());

    spiration::window_params params;
    params.title = spiration::i18n::tr("Spiration");
    params.width = 800;
    params.height = 600;
    params.decorated = false;

    auto window = spiration::window::create(params);
    if (!window) {
        spiration::console::error("failed to create window");
        return 1;
    }

    auto root = std::make_unique<spiration::root>(window);
    spiration::root* root_ptr = root.get();

    auto api = std::make_shared<spiration::extension_api>(window);
    api->set_root(root_ptr);
    spiration::extension_manager::initialize(api);

    std::string extDir = spiration::platform::extension_directory();
    size_t extCount = spiration::extension_manager::load_extensions_from(extDir);
    spiration::console::info("loaded %zu extension(s)", extCount);

    size_t initCount = spiration::extension_manager::initialize_all();
    spiration::console::info("initialized %zu extension(s)", initCount);

    window->set_widget(std::move(root));
    window->show();
    
    while (!window->should_close()) {
        window->loop();
    }

    spiration::extension_manager::shutdown();

    return 0;
}