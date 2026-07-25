/**
 * @file about_tab.cpp
 * @brief "关于"标签页实现。
 * @author clk
 */

#include <ui/about_tab.h>
#include <ui/theme_manager.h>
#include <utils/i18n.h>
#include <utils/platform.h>

namespace spiration {

about_tab::about_tab() {
    title_ = i18n_manager::tr("about");
    app_name_ = "Spiration";
    app_version_ = "1.0.0";
    platform_name_ = platform::os_name() + " (" + platform::architecture() + ")";
}

void about_tab::paint(std::shared_ptr<renderer> renderer) {
    float left = x + 24.0f;
    float right = x + width - 24.0f;
    float top = y + 24.0f;
    float content_w = right - left;

    renderer->draw_text_aligned(
        app_name_,
        {left, top, content_w, 40.0f},
        theme_manager::get(theme_manager::POPUP_TEXT),
        text_alignment::left, vertical_alignment::center, 28.0f);

    float cur_y = top + 50.0f;

    renderer->draw_text_aligned(
        i18n_manager::tr("about_version") + " " + app_version_,
        {left, cur_y, content_w, 22.0f},
        theme_manager::get(theme_manager::SEPARATOR),
        text_alignment::left, vertical_alignment::center, 15.0f);
    cur_y += 28.0f;

    renderer->draw_text_aligned(
        i18n_manager::tr("about_platform", "Platform") + ": " + platform_name_,
        {left, cur_y, content_w, 22.0f},
        theme_manager::get(theme_manager::SEPARATOR),
        text_alignment::left, vertical_alignment::center, 15.0f);
    cur_y += 28.0f;

    cur_y += 8.0f;
    renderer->draw_line(
        {left, cur_y},
        {right, cur_y},
        theme_manager::get(theme_manager::SEPARATOR), 1.0f);
    cur_y += 16.0f;

    renderer->draw_text_aligned(
        i18n_manager::tr("about_desc"),
        {left, cur_y, content_w, 44.0f},
        theme_manager::get(theme_manager::POPUP_TEXT),
        text_alignment::left, vertical_alignment::top, 14.0f);
    cur_y += 52.0f;

    renderer->draw_text_aligned(
        i18n_manager::tr("about_copyright"),
        {left, cur_y, content_w, 20.0f},
        theme_manager::get(theme_manager::TEXT_MUTED),
        text_alignment::left, vertical_alignment::center, 12.0f);
}

void about_tab::handle_event(const event_type& type, void* data) {
    if (type == event_type::mouse) {
        return;
    }
    container::handle_event(type, data);
}

} // namespace spiration
