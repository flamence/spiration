/**
 * @file appbar.cpp
 * @brief 应用顶部菜单栏控件实现。
 * @author clk
 */

#include <ui/appbar.h>
#include <ui/edit_tab.h>
#include <ui/menu_bar.h>
#include <ui/root.h>
#include <ui/spacer.h>
#include <ui/window_controls.h>
#include <utils/console.h>
#include <ui/theme.h>
#ifdef _WIN32
#include <windows.h>
#include <cstring>
#endif

namespace spiration {

void appbar::init() {
    style.background_color = theme::appbar_bg();
    height = 34;

    auto hlayout = std::make_unique<horizontal_layout>(0.0f);
    set_layout_manager(std::move(hlayout));

    
    auto mbar = std::make_unique<menu_bar>();
    mbar->height = height;
    mbar->init();

    { auto& m = mbar->add_menu("file");
      m.sub_items.push_back({"new",   [mbar = mbar.get()](){
          
          for (auto* p = mbar->parent(); p; p = p->parent()) {
              if (auto* r = dynamic_cast<root*>(p)) {
                  auto et = std::make_unique<edit_tab>();
                  et->new_file("untitled");
                  r->get_tab_bar()->add_tab(std::move(et));
                  break;
              }
          }
      }});
      m.sub_items.push_back({"open",  [mbar = mbar.get()](){
          OPENFILENAMEA ofn{};
          char path[MAX_PATH] = {};
          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = nullptr;
          ofn.lpstrFilter = "All Files\0*.*\0";
          ofn.lpstrFile = path;
          ofn.nMaxFile = MAX_PATH;
          ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
          if (GetOpenFileNameA(&ofn)) {
              for (auto* p = mbar->parent(); p; p = p->parent()) {
                  if (auto* r = dynamic_cast<root*>(p)) {
                      auto et = std::make_unique<edit_tab>();
                      et->open(path);
                      r->get_tab_bar()->add_tab(std::move(et));
                      break;
                  }
              }
          }
      }});
      m.sub_items.push_back({"save",  [](){ console::info("Save"); }});
      m.sub_items.push_back({"---", nullptr});
      m.sub_items.push_back({"exit",  [mbar = mbar.get()](){ auto cb = mbar->get_window_action_callback(); if (cb) cb(widget::action_close); }}); }

    { auto& m = mbar->add_menu("edit");
      m.sub_items.push_back({"undo", [](){ console::info("Undo"); }});
      m.sub_items.push_back({"redo", [](){ console::info("Redo"); }});
      m.sub_items.push_back({"---", nullptr});
      m.sub_items.push_back({"cut",   [](){ console::info("Cut"); }});
      m.sub_items.push_back({"copy",  [](){ console::info("Copy"); }});
      m.sub_items.push_back({"paste", [](){ console::info("Paste"); }}); }

    { auto& m = mbar->add_menu("view");
      m.sub_items.push_back({"fullscreen", [](){ console::info("Fullscreen"); }});
      m.sub_items.push_back({"---", nullptr});
      m.sub_items.push_back({"zoom_in",  [](){ console::info("Zoom In"); }});
      m.sub_items.push_back({"zoom_out", [](){ console::info("Zoom Out"); }}); }

    { auto& m = mbar->add_menu("help");
      m.sub_items.push_back({"about", [](){ console::info("About Spiration"); }}); }

    mbar->style.width = static_cast<int>(mbar->children().size()) * 60;
#ifdef __APPLE__
    
    auto wc = std::make_unique<window_controls>();
    wc->height = height;
    wc->init();
    wc->style.width = 46 * 3;
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
    wc->style.width = 46 * 3;
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