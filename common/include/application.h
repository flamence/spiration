/**
 * @file application.h
 * @brief 应用定义。
 * @author clk
 */

#pragma once

#include <extension/extension_manager.h>
#include <memory>
#include <ui/root.h>
#include <utils/platform.h>
#include <window/window.h>

namespace spiration {

class application {
public:
    void initialize();
    spiration::extension_manager* extension() const;
    static application* instance();

    void loop();
    void shutdown();
    spiration::widget* widget() const;
    spiration::window* window() const;

private:
    static std::unique_ptr<application> instance_;
    std::shared_ptr<spiration::window> window_;
    spiration::widget* widget_;

    std::shared_ptr<spiration::window> create_window();
};

}