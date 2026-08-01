/**
 * @file theme_manager.cpp
 * @brief 基于 profile 的主题管理器实现。
 * @author clk
 */

#include <ui/theme_manager.h>

namespace spiration {

std::vector<theme_manager::profile> theme_manager::s_profiles;
size_t theme_manager::s_active = 0;

void theme_manager::ensure_profiles() {
    if (!s_profiles.empty()) return;

    s_profiles.resize(1);
    s_profiles[0].name = "dark";
    init_defaults(&s_profiles[0]);
}

void theme_manager::init_defaults(profile* dark) {
    auto d = [&](const char* k, color v) { dark->params[k] = v; };

    d(WINDOW_BG,          {0.18f, 0.18f, 0.18f});
    d(APPBAR_BG,          {0.15f, 0.15f, 0.15f});
    d(MENU_BAR_BG,        {0.15f, 0.15f, 0.15f});
    d(MENU_TEXT,          {0.85f, 0.85f, 0.85f});
    d(BUTTON_TEXT,        {0.85f, 0.85f, 0.85f});
    d(BUTTON_HOVER,       {1.00f, 1.00f, 1.00f, 0.12f});
    d(BUTTON_PRESS,       {1.00f, 1.00f, 1.00f, 0.22f});
    d(POPUP_BG,           {0.22f, 0.22f, 0.22f});
    d(POPUP_BORDER,       {0.35f, 0.35f, 0.35f});
    d(POPUP_HOVER,        {0.30f, 0.50f, 0.90f, 0.35f});
    d(POPUP_TEXT,         {0.85f, 0.85f, 0.85f});
    d(CLOSE_HOVER,        {0.85f, 0.25f, 0.25f});
    d(CONTROL_ICON,       {0.70f, 0.70f, 0.70f});
    d(CONTROL_ICON_HOVER, {1.00f, 1.00f, 1.00f});
    d(CONTROL_HOVER_BG,   {1.00f, 1.00f, 1.00f, 0.08f});
    d(SEPARATOR,          {0.35f, 0.35f, 0.35f});

    d(CONTENT_BG,         {0.20f, 0.20f, 0.20f});
    d(TEXT_MUTED,         {0.45f, 0.45f, 0.45f});

    d(CODE_BG,            {0.12f, 0.12f, 0.13f});
    d(CODE_TEXT,          {0.80f, 0.80f, 0.82f});
    d(LINK_TEXT,          {0.45f, 0.70f, 1.00f});
    d(HEADING_TEXT,       {0.93f, 0.93f, 0.93f});
    d(QUOTE_BAR,          {0.45f, 0.45f, 0.45f});

    d(TAB_BAR_BG,          {0.17f, 0.17f, 0.17f});
    d(TAB_ACTIVE_BG,       {0.13f, 0.13f, 0.13f});
    d(TAB_ACTIVE_TEXT,     {0.85f, 0.85f, 0.85f});
    d(TAB_HOVER_BG,        {0.22f, 0.22f, 0.22f});
    d(TAB_HOVER_TEXT,      {0.70f, 0.70f, 0.70f});
    d(TAB_INACTIVE_BG,     {0.19f, 0.19f, 0.19f});
    d(TAB_INACTIVE_TEXT,   {0.50f, 0.50f, 0.50f});
    d(TAB_INDICATOR,       {0.29f, 0.55f, 0.97f});
    d(TAB_CLOSE_FG,        {0.50f, 0.50f, 0.50f});
    d(TAB_CLOSE_HOVER_FG,  {0.85f, 0.20f, 0.20f});

    d(LABEL_TEXT,           {0.85f, 0.85f, 0.85f});
    d(CHECKBOX_BORDER,      {0.45f, 0.45f, 0.45f});
    d(CHECKBOX_CHECK_BG,    {0.29f, 0.55f, 0.97f});
    d(CHECKBOX_CHECK_FG,    {1.00f, 1.00f, 1.00f});
    d(TOGGLE_BG,            {0.35f, 0.35f, 0.35f});
    d(TOGGLE_BG_ACTIVE,     {0.29f, 0.55f, 0.97f});
    d(TOGGLE_KNOB,          {1.00f, 1.00f, 1.00f});
    d(SLIDER_TRACK,         {0.30f, 0.30f, 0.30f});
    d(SLIDER_FILL,          {0.29f, 0.55f, 0.97f});
    d(SLIDER_THUMB,         {0.85f, 0.85f, 0.85f});
    d(SLIDER_THUMB_HOVER,   {1.00f, 1.00f, 1.00f});
    d(INPUT_BG,             {0.12f, 0.12f, 0.12f});
    d(INPUT_BORDER,         {0.35f, 0.35f, 0.35f});
    d(INPUT_FOCUS_BORDER,   {0.29f, 0.55f, 0.97f});
    d(INPUT_TEXT,           {0.85f, 0.85f, 0.85f});
    d(INPUT_PLACEHOLDER,    {0.45f, 0.45f, 0.45f});
    d(INPUT_CURSOR,         {0.85f, 0.85f, 0.85f});
    d(PROGRESS_BG,          {0.25f, 0.25f, 0.25f});
    d(PROGRESS_FILL,        {0.29f, 0.55f, 0.97f});

    d(TOOLTIP_BG,             {0.15f, 0.15f, 0.15f});
    d(TOOLTIP_TEXT,           {0.90f, 0.90f, 0.90f});
    d(COMBO_BG,               {0.12f, 0.12f, 0.12f});
    d(COMBO_BORDER,           {0.35f, 0.35f, 0.35f});
    d(COMBO_ARROW,            {0.60f, 0.60f, 0.60f});
    d(SPLIT_HANDLE,           {0.25f, 0.25f, 0.25f});
    d(SPLIT_HANDLE_HOVER,     {0.29f, 0.55f, 0.97f});
    d(DIALOG_BG,              {0.18f, 0.18f, 0.20f});
    d(DIALOG_OVERLAY,         {0.00f, 0.00f, 0.00f, 0.40f});
    d(LIST_BG,                {0.14f, 0.14f, 0.16f});
    d(LIST_ITEM_HOVER,        {0.22f, 0.22f, 0.26f});
    d(LIST_ITEM_SELECTED,     {0.25f, 0.45f, 0.80f, 0.40f});
    d(SCROLL_BAR_BG,          {0.10f, 0.10f, 0.12f});
    d(SCROLL_BAR_THUMB,       {0.30f, 0.30f, 0.35f});
    d(SCROLL_BAR_THUMB_HOVER, {0.45f, 0.45f, 0.50f});

    auto ds = [&](const char* k, const std::string& v) { dark->strings[k] = v; };
    ds(UI_FONT,     "");
    ds(INPUT_FONT,  "");
    ds(EDITOR_FONT, "Consolas");
}

void theme_manager::set_active(const std::string& name) {
    ensure_profiles();
    for (size_t i = 0; i < s_profiles.size(); ++i) {
        if (s_profiles[i].name == name) {
            s_active = i;
            return;
        }
    }
}

std::string theme_manager::active() {
    ensure_profiles();
    return s_profiles[s_active].name;
}

std::vector<std::string> theme_manager::profiles() {
    ensure_profiles();
    std::vector<std::string> names;
    names.reserve(s_profiles.size());
    for (const auto& p : s_profiles)
        names.push_back(p.name);
    return names;
}

void theme_manager::register_profile(const std::string& name) {
    ensure_profiles();
    for (const auto& p : s_profiles) {
        if (p.name == name) return;
    }
    s_profiles.push_back({name, {}, {}});
}

color theme_manager::get(const std::string& key) {
    ensure_profiles();
    auto& p = s_profiles[s_active];
    auto it = p.params.find(key);
    return it != p.params.end() ? it->second : color{};
}

void theme_manager::set(const std::string& key, const color& value) {
    ensure_profiles();
    s_profiles[s_active].params[key] = value;
}

void theme_manager::set(const std::string& profile_name, const std::string& key, const color& value) {
    ensure_profiles();
    for (auto& p : s_profiles) {
        if (p.name == profile_name) {
            p.params[key] = value;
            return;
        }
    }
}

std::string theme_manager::get_str(const std::string& key, const std::string& fallback) {
    ensure_profiles();
    auto& p = s_profiles[s_active];
    auto it = p.strings.find(key);
    return it != p.strings.end() ? it->second : fallback;
}

void theme_manager::set_str(const std::string& key, const std::string& value) {
    ensure_profiles();
    s_profiles[s_active].strings[key] = value;
}

void theme_manager::set_str(const std::string& profile_name, const std::string& key, const std::string& value) {
    ensure_profiles();
    for (auto& p : s_profiles) {
        if (p.name == profile_name) {
            p.strings[key] = value;
            return;
        }
    }
}

} // namespace spiration
