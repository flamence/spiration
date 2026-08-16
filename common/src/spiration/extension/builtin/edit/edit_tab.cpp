#include <spiration/application.h>
#include <spiration/extension/builtin/edit/edit_tab.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <filesystem>
#include <spiration/ui/theme_manager.h>
#include <algorithm>
#include <format>
#include <fstream>
#include <string>

namespace spiration {
namespace edit {

edit_tab::edit_tab() {
    base_title_ = i18n_manager::get().tr("edit.untitled");
    title_ = base_title_;
    widget_style.background_color = theme_manager::get(theme_manager::WINDOW_BG);

    auto ed = std::make_unique<text_area>();
    ed->show_line_numbers = true;
    ed->font_size = 15.0f;
    ed->on_changed = [this](const std::string&) { mark_dirty(); };
    ed->on_save = [this]() { if (save_cb_) save_cb_(); };
    editor_ = ed.get();
    add_child(std::move(ed));
}

edit_tab::edit_tab(const std::string& title) {
    base_title_ = title;
    title_ = base_title_;
    widget_style.background_color = theme_manager::get(theme_manager::WINDOW_BG);

    auto ed = std::make_unique<text_area>();
    ed->show_line_numbers = true;
    ed->font_size = 15.0f;
    ed->on_changed = [this](const std::string&) { mark_dirty(); };
    ed->on_save = [this]() { if (save_cb_) save_cb_(); };
    editor_ = ed.get();
    add_child(std::move(ed));
}

void edit_tab::mark_dirty() {
    if (!dirty_) { dirty_ = true; update_title(); }
}

void edit_tab::mark_clean() {
    if (dirty_) dirty_ = false;
    update_title();
}

void edit_tab::update_title() {
    title_ = dirty_ ? "*" + base_title_ : base_title_;
    if (on_title_change_) on_title_change_(title_);
}

void edit_tab::set_text(const std::string& text) {
    editor_->text = text;
    editor_->reset_view();
    mark_clean();
}

void edit_tab::load_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        application::instance()->logger()->log({ .level = logging::level::warning, .message = std::format("failed to open: {}", path.string()), .tag = "edit" });
        return;
    }

    std::string buf;
    char chunk[65536];
    while (file.read(chunk, sizeof(chunk)) || file.gcount() > 0) {
        buf.append(chunk, static_cast<size_t>(file.gcount()));
        if (buf.size() > MAX_FILE_SIZE) {
            application::instance()->logger()->log({ .level = logging::level::warning, .message = std::format("file too large (>100MB): {}", path.string()), .tag = "edit" });
            return;
        }
    }
    file.close();

    title_ = path.filename().string();

    editor_->text = std::move(buf);
    editor_->reset_view();
    file_path_ = path;
    mark_clean();

    application::instance()->logger()->log({ .level = logging::level::info, .message = std::format("loaded {} bytes: {}", editor_->text.size(), path.string()), .tag = "edit" });
}

void edit_tab::open_content(const std::filesystem::path& path, const std::string& content) {
    base_title_ = path.filename().string();
    title_ = base_title_;

    editor_->text = content;
    editor_->reset_view();
    file_path_ = path;
    mark_clean();

    application::instance()->logger()->log({ .level = logging::level::info, .message = std::format("opened content {} bytes: {}", content.size(), path.string()), .tag = "edit" });
}

bool edit_tab::save() {
    if (file_path_.empty()) return false;
    return save_as(file_path_);
}

bool edit_tab::save_as(const std::filesystem::path& path) {
    std::filesystem::path tmp_fs_path = path;
    tmp_fs_path += ".tmp";
    std::ofstream file(tmp_fs_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        application::instance()->logger()->log({ .level = logging::level::warning, .message = std::format("save_as: cannot open for write: {}", path.string()), .tag = "edit" });
        return false;
    }

    constexpr size_t CHUNK = 65536;
    size_t remaining = editor_->text.size();
    size_t offset = 0;
    while (remaining > 0) {
        size_t to_write = std::min(remaining, static_cast<size_t>(CHUNK));
        file.write(editor_->text.data() + offset, static_cast<std::streamsize>(to_write));
        if (!file) {
            file.close();
            std::filesystem::remove(tmp_fs_path);
            application::instance()->logger()->log({ .level = logging::level::warning, .message = std::format("save_as: write error at offset {}", offset), .tag = "edit" });
            return false;
        }
        offset += to_write;
        remaining -= to_write;
    }

    file.close();

    std::error_code ec;
    std::filesystem::rename(tmp_fs_path, path, ec);
    if (ec) {
        application::instance()->logger()->log({ .level = logging::level::warning, .message = std::format("save_as: rename failed: {}", ec.message()), .tag = "edit" });
        std::filesystem::remove(tmp_fs_path);
        return false;
    }

    file_path_ = std::move(path);

    base_title_ = path.filename().string().c_str();
    mark_clean();

    application::instance()->logger()->log({ .level = logging::level::info, .message = std::format("saved {} bytes to: {}", editor_->text.size(), file_path_.string()), .tag = "edit" });
    return true;
}

void edit_tab::confirm_saved(const std::filesystem::path& path) {
    file_path_ = path;

    base_title_ = path.filename().string();
    mark_clean();

    application::instance()->logger()->log({ .level = logging::level::info, .message = std::format("confirmed saved to: {}", path.string()), .tag = "edit" });
}

void edit_tab::paint(std::shared_ptr<renderer> r) {
    if (widget_style.background_color.a > 0.0f) {
        r->draw_rectangle({0, 0, width, height}, widget_style.background_color);
    }
    widget::paint(r);
}

void edit_tab::layout() {
    for (auto& child : children()) {
        child->x = 0;
        child->y = 0;
        child->width = width;
        child->height = height;
        child->layout();
    }
}

} // namespace edit
} // namespace spiration
