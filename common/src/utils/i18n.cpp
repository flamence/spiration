/**
 * @file i18n.cpp
 * @brief 国际化翻译管理器实现。
 * @author clk
 */

#include <utils/i18n.h>
#include <utils/console.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace spiration {

std::unordered_map<std::string, i18n::locale_data> i18n::s_translations;
std::string i18n::s_current_locale = "zh-CN";

bool i18n::load(const std::string& locale, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        console::warning("i18n: cannot open file '%s' for locale '%s'",
                         filepath.c_str(), locale.c_str());
        return false;
    }

    locale_data data;
    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        ++lineNum;

        
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                [](unsigned char c) { return !std::isspace(c); }));
            s.erase(std::find_if(s.rbegin(), s.rend(),
                [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
        };
        trim(line);

        
        if (line.empty() || line[0] == '#') continue;

        
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            console::warning("i18n: invalid syntax at line %d in '%s'",
                             lineNum, filepath.c_str());
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        trim(key);
        trim(value);

        if (key.empty()) {
            console::warning("i18n: empty key at line %d in '%s'",
                             lineNum, filepath.c_str());
            continue;
        }

        data.strings[key] = value;
    }

    s_translations[locale] = std::move(data);
    console::info("i18n: loaded %zu strings for locale '%s' from '%s'",
                  s_translations[locale].strings.size(),
                  locale.c_str(), filepath.c_str());
    return true;
}

void i18n::set_locale(const std::string& locale) {
    s_current_locale = locale;
    console::info("i18n: switched to locale '%s'", locale.c_str());
}

const std::string& i18n::get_locale() {
    return s_current_locale;
}

std::string i18n::tr(const std::string& key, const std::string& default_value) {
    
    auto it = s_translations.find(s_current_locale);
    if (it != s_translations.end()) {
        auto strIt = it->second.strings.find(key);
        if (strIt != it->second.strings.end()) {
            return strIt->second;
        }
    }

    
    if (s_current_locale != "en-US") {
        auto fallback = s_translations.find("en-US");
        if (fallback != s_translations.end()) {
            auto strIt = fallback->second.strings.find(key);
            if (strIt != fallback->second.strings.end()) {
                return strIt->second;
            }
        }
    }

    
    return default_value.empty() ? key : default_value;
}

void i18n::clear() {
    s_translations.clear();
    s_current_locale = "en-US";
}

std::vector<std::string> i18n::available_locales() {
    std::vector<std::string> locales;
    for (const auto& pair : s_translations) {
        locales.push_back(pair.first);
    }
    return locales;
}

} 
