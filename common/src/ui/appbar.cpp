/**
 * @file appbar.cpp
 * @brief 应用顶部菜单栏控件实现。
 * @author clk
 */

#include <ui/about_tab.h>
#include <ui/appbar.h>
#include <ui/extension_tab.h>
#include <ui/menu_bar.h>
#include <ui/root.h>
#include <ui/spacer.h>
#include <ui/window_controls.h>
#include <utils/console.h>
#include <application.h>
#include <extension/builtin/i18n/i18n.h>
#include <ui/theme_manager.h>
#ifdef _WIN32
#include <windows.h>
#include <cstring>
#endif

namespace spiration {

void appbar::init() {
    widget_style.background_color = theme_manager::get(theme_manager::APPBAR_BG);
    height = 34;

    auto hlayout = std::make_unique<horizontal_layout>(0.0f);
    set_layout_manager(std::move(hlayout));

    
    auto mbar = std::make_unique<menu_bar>();
    mbar->height = height;
    mbar->init();

    { auto& m = mbar->add_menu("menu.file");
      m.sub_items.push_back({i18n_manager::get().tr("menu.file.exit"),  [mbar = mbar.get()](){
          auto cb = mbar->get_window_action_callback();
          if (cb) cb(widget::action_close);
      }}); }

    { auto& m = mbar->add_menu("menu.help");
      m.sub_items.push_back({i18n_manager::get().tr("menu.help.about"), [mbar = mbar.get()](){
          for (auto* p = mbar->parent(); p; p = p->parent()) {
              if (auto* r = dynamic_cast<root*>(p)) {
                  auto at = std::make_unique<about_tab>();
                  r->get_tab_bar()->add_tab(std::move(at));
                  break;
              }
          }
      }});
      m.sub_items.push_back({i18n_manager::get().tr("extensions"), [mbar = mbar.get()](){
          for (auto* p = mbar->parent(); p; p = p->parent()) {
              if (auto* r = dynamic_cast<root*>(p)) {
                  auto et = std::make_unique<extension_tab>();
                  r->get_tab_bar()->add_tab(std::move(et));
                  break;
              }
          }
      }}); }

    mbar->widget_style.width = static_cast<int>(mbar->children().size()) * 60;
#ifdef __APPLE__
    auto wc = std::make_unique<window_controls>();
    wc->height = height;
    wc->init();
    wc->widget_style.width = 46 * 3;
    add_child(std::move(wc));
    add_child(std::move(mbar));
    add_child(std::make_unique<spacer>());
#else
    add_child(std::move(mbar));
    auto sp = std::make_unique<spacer>();
    sp->height = height;
    add_child(std::move(sp));
    auto wc = std::make_unique<window_controls>();
    wc->height = height;
    wc->init();
    wc->widget_style.width = 46 * 3;
    add_child(std::move(wc));
#endif
}

void appbar::layout() {
    if (auto* p = dynamic_cast<container*>(parent())) {
        width = p->width;
    }
    container::layout();
}

}