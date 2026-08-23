#include "spiration/ui/color.h"
#include <spiration/application.h>
#include <spiration/extension/builtin/edit/edit_tab.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <ghc/filesystem.hpp>
#include <spiration/ui/theme_manager.h>
#include <algorithm>
#include <fmt/format.h>
#include <fstream>
#include <string>

namespace spiration {
namespace edit {

edit_tab::edit_tab() : edit_tab(i18n_manager::get().tr("edit:untitled")) {
}

edit_tab::edit_tab(const std::string& title) {
    base_title_ = title;
    title_ = base_title_;
    widget_style.background_color = theme_manager::get(theme_manager::WINDOW_BG);

    auto ed = std::make_unique<text_area>();
    ed->widget_style.background_color = color::transparent();
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

void edit_tab::load_file(const ghc::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("failed to open: {}", path.string()), "edit"});
        return;
    }

    std::string buf;
    char chunk[65536];
    while (file.read(chunk, sizeof(chunk)) || file.gcount() > 0) {
        buf.append(chunk, static_cast<size_t>(file.gcount()));
        if (buf.size() > MAX_FILE_SIZE) {
            application::instance()->logger()->log({logging::level::warning, fmt::format("file too large (>100MB): {}", path.string()), "edit"});
            return;
        }
    }
    file.close();

    title_ = path.filename().string();

    editor_->text = std::move(buf);
    editor_->reset_view();
    file_path_ = path;
    mark_clean();

    application::instance()->logger()->log({logging::level::info, fmt::format("loaded {} bytes: {}", editor_->text.size(), path.string()), "edit"});
}

void edit_tab::open_content(const ghc::filesystem::path& path, const std::string& content) {
    base_title_ = path.filename().string();
    title_ = base_title_;

    editor_->text = content;
    editor_->reset_view();
    file_path_ = path;
    mark_clean();

    application::instance()->logger()->log({logging::level::info, fmt::format("opened content {} bytes: {}", content.size(), path.string()), "edit"});
}

bool edit_tab::save() {
    if (file_path_.empty()) return false;
    return save_as(file_path_);
}

bool edit_tab::save_as(const ghc::filesystem::path& path) {
    ghc::filesystem::path tmp_fs_path = path;
    tmp_fs_path += ".tmp";
    std::ofstream file(tmp_fs_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("save_as: cannot open for write: {}", path.string()), "edit"});
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
            ghc::filesystem::remove(tmp_fs_path);
            application::instance()->logger()->log({logging::level::warning, fmt::format("save_as: write error at offset {}", offset), "edit"});
            return false;
        }
        offset += to_write;
        remaining -= to_write;
    }

    file.close();

    std::error_code ec;
    ghc::filesystem::rename(tmp_fs_path, path, ec);
    if (ec) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("save_as: rename failed: {}", ec.message()), "edit"});
        ghc::filesystem::remove(tmp_fs_path);
        return false;
    }

    file_path_ = std::move(path);

    base_title_ = path.filename().string().c_str();
    mark_clean();

    application::instance()->logger()->log({logging::level::info, fmt::format("saved {} bytes to: {}", editor_->text.size(), file_path_.string()), "edit"});
    return true;
}

void edit_tab::confirm_saved(const ghc::filesystem::path& path) {
    file_path_ = path;

    base_title_ = path.filename().string();
    mark_clean();

    application::instance()->logger()->log({logging::level::info, fmt::format("confirmed saved to: {}", path.string()), "edit"});
}

void edit_tab::paint(std::shared_ptr<renderer> r) {
    if (widget_style.background_color.a > 0.0f) {
        r->draw_rectangle({0, 0, width, height}, widget_style.background_color);
    }
    widget::paint(r);
}

void edit_tab::layout() {
    on_layout_begin();
    for (auto& child : children()) {
        child->x = 0;
        child->y = 0;
        child->width = width;
        child->height = height;
        if (child->needs_layout()) child->layout();
    }
}

} // namespace edit
} // namespace spiration
