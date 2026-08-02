#include <extension/builtin/edit/edit_tab.h>
#include <extension/builtin/i18n/i18n.h>
#include <ui/theme_manager.h>
#include <utils/console.h>
#include <utils/path.h>
#include <algorithm>
#include <filesystem>
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

void edit_tab::load_file(const std::string& path) {
    std::ifstream file(path::u8path(path), std::ios::binary);
    if (!file) {
        spiration::console::warning("edit", "failed to open: %s", path.c_str());
        return;
    }

    std::string buf;
    char chunk[65536];
    while (file.read(chunk, sizeof(chunk)) || file.gcount() > 0) {
        buf.append(chunk, static_cast<size_t>(file.gcount()));
        if (buf.size() > MAX_FILE_SIZE) {
            spiration::console::warning("edit", "file too large (>100MB): %s", path.c_str());
            return;
        }
    }
    file.close();

    size_t sep = path.find_last_of("/\\");
    base_title_ = (sep != std::string::npos) ? path.substr(sep + 1) : path;
    title_ = base_title_;

    editor_->text = std::move(buf);
    editor_->reset_view();
    file_path_ = path;
    mark_clean();

    spiration::console::info("edit", "loaded %zu bytes: %s", editor_->text.size(), path.c_str());
}

bool edit_tab::save() {
    if (file_path_.empty()) return false;
    return save_as(file_path_);
}

bool edit_tab::save_as(const std::string& path) {
    std::string save_path = path;

    std::filesystem::path fs_path = path::u8path(save_path);
    std::filesystem::path tmp_fs_path = fs_path;
    tmp_fs_path += ".tmp";
    std::ofstream file(tmp_fs_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        spiration::console::warning("edit", "save_as: cannot open for write: %s", save_path.c_str());
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
            spiration::console::warning("edit", "save_as: write error at offset %zu", offset);
            return false;
        }
        offset += to_write;
        remaining -= to_write;
    }

    file.close();

    std::error_code ec;
    std::filesystem::rename(tmp_fs_path, fs_path, ec);
    if (ec) {
        spiration::console::warning("edit", "save_as: rename failed: %s", ec.message().c_str());
        std::filesystem::remove(tmp_fs_path);
        return false;
    }

    file_path_ = std::move(save_path);

    size_t sep = file_path_.find_last_of("/\\");
    base_title_ = (sep != std::string::npos) ? file_path_.substr(sep + 1) : file_path_;
    mark_clean();

    spiration::console::info("edit", "saved %zu bytes to: %s", editor_->text.size(), file_path_.c_str());
    return true;
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