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

std::unordered_map<std::string, i18n_manager::locale_data> i18n_manager::s_translations;
std::string i18n_manager::s_current_locale = "zh-CN";

/**
 * @brief 对 .properties 字符串中的转义序列进行反转义。
 * @param s 原始字符串
 * @return 反转义后的字符串
 */
static std::string unescape_properties(const std::string& s) {
    if (s.find('\\') == std::string::npos) return s;

    std::string result;
    result.reserve(s.length());
    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == '\\' && i + 1 < s.length()) {
            char next = s[i + 1];
            switch (next) {
                case 'n':  result += '\n'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 'f':  result += '\f'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'u': {
                    if (i + 5 < s.length()) {
                        std::string hex = s.substr(i + 2, 4);
                        bool valid = std::all_of(hex.begin(), hex.end(),
                            [](unsigned char c) { return std::isxdigit(c); });
                        if (valid) {
                            wchar_t wc = static_cast<wchar_t>(
                                std::stoi(hex, nullptr, 16));
                            if (wc < 0x80) {
                                result += static_cast<char>(wc);
                            } else if (wc < 0x800) {
                                result += static_cast<char>(0xC0 | (wc >> 6));
                                result += static_cast<char>(0x80 | (wc & 0x3F));
                            } else {
                                result += static_cast<char>(0xE0 | (wc >> 12));
                                result += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (wc & 0x3F));
                            }
                            i += 5;
                        } else {
                            result += s[i];
                        }
                    } else {
                        result += s[i];
                    }
                    break;
                }
                default:
                    result += next;
                    ++i;
                    break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

bool i18n_manager::load(const std::string& locale, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        console::warning("i18n: cannot open file '%s' for locale '%s'",
                         filepath.c_str(), locale.c_str());
        return false;
    }

    locale_data data;
    std::string line;
    std::string logical_line;
    int start_line = 0;
    int line_num = 0;

    while (std::getline(file, line)) {
        ++line_num;

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t bs_count = 0;
        for (auto it = line.rbegin();
             it != line.rend() && *it == '\\'; ++it) {
            ++bs_count;
        }
        bool has_continuation = (bs_count % 2 == 1);
        if (has_continuation) {
            line.erase(line.length() - 1);
        }

        if (logical_line.empty()) {
            start_line = line_num;
            logical_line = line;
        } else {
            size_t first = line.find_first_not_of(" \t\f");
            if (first != std::string::npos) {
                logical_line += line.substr(first);
            }
        }

        if (has_continuation) continue;

        if (logical_line.empty()
            || logical_line[0] == '#'
            || logical_line[0] == '!') {
            logical_line.clear();
            continue;
        }

        size_t sep_pos = std::string::npos;
        for (size_t i = 0; i < logical_line.length(); ++i) {
            if (logical_line[i] == '\\') { ++i; continue; }
            if (logical_line[i] == '=' || logical_line[i] == ':') {
                sep_pos = i;
                break;
            }
        }
        if (sep_pos == std::string::npos) {
            for (size_t i = 0; i < logical_line.length(); ++i) {
                if (logical_line[i] == '\\') { ++i; continue; }
                if (logical_line[i] == ' '
                    || logical_line[i] == '\t'
                    || logical_line[i] == '\f') {
                    sep_pos = i;
                    break;
                }
            }
        }

        if (sep_pos == std::string::npos) {
            console::warning("i18n: invalid syntax at line %d in '%s'",
                             start_line, filepath.c_str());
            logical_line.clear();
            continue;
        }

        std::string key = logical_line.substr(0, sep_pos);
        std::string value = logical_line.substr(sep_pos + 1);

        while (!key.empty() && (key.back() == ' '
            || key.back() == '\t' || key.back() == '\f')) {
            key.pop_back();
        }

        size_t val_start = value.find_first_not_of(" \t\f");
        if (val_start != std::string::npos) {
            value = value.substr(val_start);
        } else {
            value.clear();
        }

        key = unescape_properties(key);
        value = unescape_properties(value);

        if (key.empty()) {
            console::warning("i18n: empty key at line %d in '%s'",
                             start_line, filepath.c_str());
            logical_line.clear();
            continue;
        }

        data.strings[key] = value;
        logical_line.clear();
    }

    s_translations[locale] = std::move(data);
    console::info("i18n: loaded %zu strings for locale '%s' from '%s'",
                  s_translations[locale].strings.size(),
                  locale.c_str(), filepath.c_str());
    return true;
}

void i18n_manager::set_locale(const std::string& locale) {
    s_current_locale = locale;
    console::info("i18n: switched to locale '%s'", locale.c_str());
}

const std::string& i18n_manager::get_locale() {
    return s_current_locale;
}

std::string i18n_manager::tr(const std::string& key, const std::string& default_value) {
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

void i18n_manager::clear() {
    s_translations.clear();
    s_current_locale = "en-US";
}

std::vector<std::string> i18n_manager::available_locales() {
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

std::string i18n_manager::tr(const std::string& key,
                     const std::vector<std::string>& args,
                     const std::string& default_value) {
    std::string translated = tr(key, default_value);
    if (translated == key && default_value.empty()) {
        return translated;
    }
    return format_string(translated, args);
}

} 
