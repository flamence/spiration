/**
 * @file appbar.cpp
 * @brief 应用顶部菜单栏控件实现。
 * @author clk
 */

#include <spiration/ui/about_tab.h>
#include <spiration/ui/appbar.h>
#include <spiration/ui/extension_tab.h>
#include <spiration/ui/menu_bar.h>
#include <spiration/ui/root.h>
#include <spiration/ui/spacer.h>
#include <spiration/ui/window_controls.h>
#include <spiration/application.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/ui/theme_manager.h>
#ifdef _WIN32
#include <windows.h>
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
      }});
      m.tail_index = m.sub_items.size() - 1; }

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
      m.sub_items.push_back({i18n_manager::get().tr("tab.extensions"), [mbar = mbar.get()](){
          for (auto* p = mbar->parent(); p; p = p->parent()) {
              if (auto* r = dynamic_cast<root*>(p)) {
                  auto et = std::make_unique<extension_tab>();
                  r->get_tab_bar()->add_tab(std::move(et));
                  break;
              }
          }
      }}); }

    mbar->widget_style.width = static_cast<int>(mbar->children().size()) * 60;
    add_child(std::move(mbar));
    auto sp = std::make_unique<spacer>();
    sp->height = height;
    add_child(std::move(sp));
    auto wc = std::make_unique<window_controls>();
    wc->height = height;
    wc->init();
    wc->widget_style.width = 46 * 3;
    add_child(std::move(wc));
}

void appbar::layout() {
    if (auto* p = dynamic_cast<container*>(parent())) {
        width = p->width;
    }
    container::layout();
}

}
