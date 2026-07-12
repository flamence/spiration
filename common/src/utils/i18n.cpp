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

    if (s_current_locale != "zh-CN") {
        auto fallback = s_translations.find("zh-CN");
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

/**
 * @brief 执行参数替换的内部辅助函数。
 */
static std::string format_string(const std::string& text,
                                  const std::vector<std::string>& args) {
    if (args.empty() || text.find('{') == std::string::npos) {
        return text;
    }

    std::string result;
    size_t pos = 0;
    size_t len = text.length();

    while (pos < len) {
        size_t brace_start = text.find('{', pos);
        if (brace_start == std::string::npos) {
            result.append(text, pos, len - pos);
            break;
        }

        result.append(text, pos, brace_start - pos);

        size_t brace_end = text.find('}', brace_start);
        if (brace_end == std::string::npos) {
            result.append(text, brace_start, len - brace_start);
            break;
        }

        std::string index_str = text.substr(brace_start + 1,
                                            brace_end - brace_start - 1);

        index_str.erase(std::remove_if(index_str.begin(), index_str.end(),
                       [](unsigned char c) { return std::isspace(c); }),
                       index_str.end());

        bool is_number = !index_str.empty() &&
                         std::all_of(index_str.begin(), index_str.end(),
                         [](unsigned char c) { return std::isdigit(c); });

        if (is_number) {
            size_t idx = static_cast<size_t>(std::stoul(index_str));
            if (idx < args.size()) {
                result.append(args[idx]);
            }
        } else {
            result.append(text, brace_start, brace_end - brace_start + 1);
        }

        pos = brace_end + 1;
    }

    return result;
}

std::string i18n::tr(const std::string& key,
                     const std::vector<std::string>& args,
                     const std::string& default_value) {
    std::string translated = tr(key, default_value);
    if (translated == key && default_value.empty()) {
        return translated;
    }
    return format_string(translated, args);
}

} 
