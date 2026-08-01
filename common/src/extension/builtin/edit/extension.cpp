#include <extension/builtin/edit/extension.h>
#include <extension/extension_api.h>
#include <io/file_dialog.h>
#include <ui/root.h>
#include <ui/tab_bar.h>
#include <ui/theme_manager.h>
#include <utils/console.h>
#include <application.h>

namespace spiration {
namespace edit {

bool extension::initialize() {
    if (!api) return false;

    api->add_menu_item("menu.file", i18n_manager::get().tr("menu.file.new"), [this]() {
        new_editor_tab();
    });

    api->add_menu_item("menu.file", i18n_manager::get().tr("menu.file.open"), [this]() {
        open_editor_tab();
    });

    api->add_menu_item("menu.file", i18n_manager::get().tr("menu.file.save"), [this]() {
        save_current();
    });

    api->add_menu_item("menu.file", i18n_manager::get().tr("menu.file.save_as"), [this]() {
        save_current_as();
    });

    return true;
}

void extension::shutdown() {
}

edit_tab* extension::active_editor() {
    for (auto* e : editors_) {
        if (e->is_active()) return e;
    }
    return nullptr;
}

void extension::setup_editor_callbacks(edit_tab* tab) {
    tab->set_repaint_callback([this]() { api->request_repaint(); });
    tab->set_save_callback([this]() { save_current(); });
    tab->set_activate_callback([this, tab]() {
    });
    editors_.push_back(tab);
}

void extension::new_editor_tab() {
    auto tab = std::make_unique<edit_tab>("Untitled");

    auto* raw = tab.get();
    setup_editor_callbacks(raw);
    api->log_info("created new tab");
    api->open_tab(std::move(tab));
}

void extension::open_editor_tab() {
    std::string path = spiration::io::open_file(
        "选择要编辑的文件",
        "All Files (*.*)",
        {"*"}
    );

    if (path.empty()) {
        api->log_info("file dialog cancelled");
        return;
    }

    spiration::root* root = dynamic_cast<spiration::root*>(
        spiration::application::instance()->widget());
    if (root) {
        auto* tb = root->get_tab_bar();
        for (int i = 0; i < tb->tab_count(); ++i) {
            auto* existing = dynamic_cast<edit_tab*>(tb->get_tab(i));
            if (existing && existing->file_path() == path) {
                tb->activate_tab(i);
                api->log_info("switched to already-open tab: %s", path.c_str());
                return;
            }
        }
    }

    std::string filename = path;
    size_t sep = path.find_last_of("/\\");
    if (sep != std::string::npos) {
        filename = path.substr(sep + 1);
    }

    auto tab = std::make_unique<edit_tab>(filename);
    tab->load_file(path);

    auto* raw = tab.get();
    setup_editor_callbacks(raw);
    api->log_info("opened file: %s", path.c_str());
    api->open_tab(std::move(tab));
}

void extension::save_current() {
    auto* editor = active_editor();
    if (!editor) {
        api->log_info("no active editor to save");
        return;
    }
    if (editor->save()) {
        api->log_info("saved: %s", editor->file_path().c_str());
    } else if (editor->file_path().empty()) {
        save_current_as();
    } else {
        api->log_warning("save failed: %s", editor->file_path().c_str());
    }
}

void extension::save_current_as() {
    auto* editor = active_editor();
    if (!editor) {
        api->log_info("no active editor to save");
        return;
    }

    std::string path = spiration::io::save_file(
        "另存为",
        "All Files (*.*)",
        {"*"}
    );

    if (path.empty()) {
        api->log_info("save-as cancelled");
        return;
    }

    if (editor->save_as(path)) {
        api->log_info("saved as: %s", path.c_str());
    } else {
        api->log_warning("save-as failed: %s", path.c_str());
    }
}

} // namespace edit
} // namespace spiration
