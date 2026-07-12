#pragma once

#include <memory>
#include <functional>
#include <ui/appbar.h>
#include <ui/container.h>
#include <ui/menu_bar.h>
#include <ui/popup_menu.h>
#include <ui/tab_bar.h>
#include <window/window.h>

namespace spiration {

class root : public container {
private:
    std::shared_ptr<spiration::window> m_window;
    std::unique_ptr<popup_menu> m_popup = nullptr;

public:
    explicit root(std::shared_ptr<spiration::window> parent);

    void init() override;
    void layout() override;
    void paint(std::shared_ptr<renderer> renderer) override;
    void tick(float dt_ms) override;
    void handle_event(const event_type& type, void* data) override;

    void show_popup(float x, float y, std::unique_ptr<popup_menu> popup);
    void dismiss_popup();
    bool has_popup() const { return m_popup != nullptr; }

    tab_bar* get_tab_bar() const { return tab_bar_; }
    menu_bar* get_menu_bar() const { return menu_bar_; }
    std::shared_ptr<spiration::window> window() const { return m_window; }

    /**
     * @brief 在指定菜单中添加子项。
     */
    bool add_menu_item(const std::string& menu_name,
                       const std::string& label,
                       std::function<void()> callback);

    /**
     * @brief 在标签栏中打开一个新标签页。
     */
    void open_tab(std::unique_ptr<tab> t);

private:
    tab_bar* tab_bar_ = nullptr;
    menu_bar* menu_bar_ = nullptr;
};

} 
