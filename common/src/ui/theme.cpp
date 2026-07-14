/**
 * @file theme.cpp
 * @brief 基于 profile 的主题系统实现。
 * @author clk
 */

#include <ui/theme.h>

namespace spiration {

std::vector<theme::profile> theme::s_profiles;
size_t theme::s_active = 0;

void theme::ensure_profiles() {
    if (!s_profiles.empty()) return;

    s_profiles.resize(1);
    s_profiles[0].name = "dark";
    init_defaults(&s_profiles[0]);
}

void theme::init_defaults(profile* dark) {
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
}

void theme::set_active(const std::string& name) {
    ensure_profiles();
    for (size_t i = 0; i < s_profiles.size(); ++i) {
        if (s_profiles[i].name == name) {
            s_active = i;
            return;
        }
    }
}

std::string theme::active() {
    ensure_profiles();
    return s_profiles[s_active].name;
}

std::vector<std::string> theme::profiles() {
    ensure_profiles();
    std::vector<std::string> names;
    names.reserve(s_profiles.size());
    for (const auto& p : s_profiles)
        names.push_back(p.name);
    return names;
}

void theme::register_profile(const std::string& name) {
    ensure_profiles();
    for (const auto& p : s_profiles) {
        if (p.name == name) return;
    }
    s_profiles.push_back({name, {}});
}

color theme::get(const std::string& key) {
    ensure_profiles();
    auto& p = s_profiles[s_active];
    auto it = p.params.find(key);
    return it != p.params.end() ? it->second : color{};
}

void theme::set(const std::string& key, const color& value) {
    ensure_profiles();
    s_profiles[s_active].params[key] = value;
}

void theme::set(const std::string& profile, const std::string& key, const color& value) {
    ensure_profiles();
    for (auto& p : s_profiles) {
        if (p.name == profile) {
            p.params[key] = value;
            return;
        }
    }
}

} 
