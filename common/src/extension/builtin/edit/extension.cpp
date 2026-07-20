/**
 * @file extension.cpp
 * @brief 拓展入口。
 * @author clk
 */

#include <extension/builtin/edit/extension.h>
#include <extension/extension_api.h>
#include <io/file_dialog.h>
#include <ui/theme_manager.h>
#include <utils/console.h>

namespace spiration {
namespace edit {

bool extension::initialize() {
    if (!api_) return false;

    api_->log_info("edit_extension: initializing...");

    api_->add_menu_item("file", "新建", [this]() {
        new_editor_tab();
    });

    api_->add_menu_item("file", "打开...", [this]() {
        open_editor_tab();
    });

    api_->add_menu_item("file", "保存", [this]() {
        save_current();
    });

    api_->add_menu_item("file", "另存为...", [this]() {
        save_current_as();
    });

    api_->log_info("initialized successfully");
    return true;
}

void extension::shutdown() {
    api_->log_info("shutting down...");
}

edit_tab* extension::active_editor() {
    for (auto* e : editors_) {
        if (e->is_active()) return e;
    }
    return nullptr;
}

void extension::setup_editor_callbacks(edit_tab* tab) {
    tab->set_repaint_callback([this]() { api_->request_repaint(); });
    tab->set_save_callback([this]() { save_current(); });
    tab->set_activate_callback([this, tab]() {
    });
    editors_.push_back(tab);
}

void extension::new_editor_tab() {
    auto tab = std::make_unique<edit_tab>("Untitled");

    auto* raw = tab.get();
    setup_editor_callbacks(raw);
    api_->log_info("created new tab");
    api_->open_tab(std::move(tab));
}

void extension::open_editor_tab() {
    std::string path = spiration::io::open_file(
        "选择要编辑的文件",
        "All Files (*.*)",
        {"*"}
    );

    if (path.empty()) {
        api_->log_info("file dialog cancelled");
        return;
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
    api_->log_info("opened file: %s", path);
    api_->open_tab(std::move(tab));
}

void extension::save_current() {
    auto* editor = active_editor();
    if (!editor) {
        api_->log_info("no active editor to save");
        return;
    }
    if (editor->save()) {
        api_->log_info("saved: %s", editor->file_path());
    } else if (editor->file_path().empty()) {
        save_current_as();
    } else {
        api_->log_warning("save failed: %s", editor->file_path());
    }
}

void extension::save_current_as() {
    auto* editor = active_editor();
    if (!editor) {
        api_->log_info("no active editor to save");
        return;
    }

    std::string path = spiration::io::save_file(
        "另存为",
        "All Files (*.*)",
        {"*"}
    );

    if (path.empty()) {
        api_->log_info("save-as cancelled");
        return;
    }

    if (editor->save_as(path)) {
        api_->log_info("saved as: %s", path);
    } else {
        api_->log_warning("save-as failed: %s", path);
    }
}

} // namespace edit
} // namespace spiration
