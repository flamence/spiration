#include <spiration/extension/builtin/edit/extension.h>
#include <spiration/extension/extension_api.h>
#include <fmt/format.h>
#include <spiration/io/file_dialog.h>
#include <spiration/ui/root.h>
#include <spiration/ui/tab_bar.h>
#include <spiration/ui/theme_manager.h>
#include <spiration/application.h>
#include <spiration/utils/io.h>

namespace spiration {
namespace edit {

bool extension::initialize() {
    if (!api) return false;

    api->add_menu_item("menu.file", i18n_manager::get().tr("edit:menu.file.new"), [this]() {
        new_editor_tab();
    });

    api->add_menu_item("menu.file", i18n_manager::get().tr("edit:menu.file.open"), [this]() {
        open_editor_tab();
    });

    api->add_menu_item("menu.file", i18n_manager::get().tr("edit:menu.file.save"), [this]() {
        save_current();
    });

    api->add_menu_item("menu.file", i18n_manager::get().tr("edit:menu.file.save_as"), [this]() {
        save_current_as();
    });

    // 注册编辑服务，供其它拓展（如智能体）将文件打开到编辑器。
    application::instance()->extension()->register_service(
        id(), edit_service::SERVICE_NAME, static_cast<edit_service*>(this));

    return true;
}

void extension::open_file(const ghc::filesystem::path& path) {
#if defined(OHOS_PLATFORM)
    std::string content = spiration::io::read_file_content(path);
    complete_open_editor_tab(path.string(), content);
#else
    complete_open_editor_tab(path.string());
#endif
}

void extension::open_file_with_content(const ghc::filesystem::path& path,
                                       const std::string& content) {
    complete_open_editor_tab(path.string(), content);
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
    auto tab = std::make_unique<edit_tab>(api->tr("edit:untitled"));

    auto* raw = tab.get();
    setup_editor_callbacks(raw);
    api->log_info("created new tab");
    api->open_tab(std::move(tab));
}

void extension::open_editor_tab() {
#if __OHOS__
    spiration::io::open_file_async(api->tr("edit:file_dialog.open_title"),
                                   api->tr("edit:file_dialog.all_files"), {"*"},
                                   [this](const std::string& path, const std::string& content) {
                                       complete_open_editor_tab(path, content);
                                   });
#else
    std::string path = spiration::io::open_file(
        api->tr("edit:file_dialog.open_title"),
        api->tr("edit:file_dialog.all_files"),
        {"*"});
    complete_open_editor_tab(path);
#endif
}

void extension::complete_open_editor_tab(const std::string& path, const std::string& content) {
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
                api->log_info(fmt::format("switched to already-open tab: {}", path));
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
#if defined(OHOS_PLATFORM)
    tab->open_content(path, content);
#else
    tab->load_file(path);
#endif

    auto* raw = tab.get();
    setup_editor_callbacks(raw);
    api->log_info(fmt::format("opened file: {}", path));
    api->open_tab(std::move(tab));
}

void extension::save_current() {
    auto* editor = active_editor();
    if (!editor) {
        api->log_info("no active editor to save");
        return;
    }
    if (editor->save()) {
        api->log_info(fmt::format("saved: {}", editor->file_path().string()));
    } else if (editor->file_path().empty()) {
        save_current_as();
    } else {
        api->log_warning(fmt::format("save failed: {}", editor->file_path().string()));
    }
}

void extension::save_current_as() {
    auto* editor = active_editor();
    if (!editor) {
        api->log_info("no active editor to save");
        return;
    }
#if __OHOS__
    spiration::io::save_file_async(api->tr("edit:file_dialog.save_title"),
                                   api->tr("edit:file_dialog.all_files"), {"*"},
                                   editor->text(),
                                   [this](const std::string& path) {
                                       complete_save_as(path);
                                   });
#else
    std::string path = spiration::io::save_file(
        api->tr("edit:file_dialog.save_title"),
        api->tr("edit:file_dialog.all_files"),
        {"*"});
    complete_save_as(path);
#endif
}

void extension::complete_save_as(const std::string& path) {
    if (path.empty()) {
        api->log_info("save-as cancelled");
        return;
    }
    auto* editor = active_editor();
    if (!editor) {
        api->log_info("no active editor to save");
        return;
    }
#if defined(OHOS_PLATFORM)
    editor->confirm_saved(path);
    api->log_info(fmt::format("saved as: {}", path));
#else
    if (editor->save_as(path)) {
        api->log_info(fmt::format("saved as: {}", path));
    } else {
        api->log_warning(fmt::format("save-as failed: {}", path));
    }
#endif
}

} // namespace edit
} // namespace spiration
