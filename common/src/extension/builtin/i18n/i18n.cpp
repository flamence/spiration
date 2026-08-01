#include <extension/builtin/i18n/i18n.h>
#include <utils/console.h>
#include <algorithm>
#include <cctype>
#include <fstream>

namespace spiration {

i18n_manager& i18n_manager::get() {
    static i18n_manager instance;
    return instance;
}

bool i18n_manager::load(const std::string& locale, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        console::warning("i18n", "cannot open '%s'", filepath.c_str());
        return false;
    }

    locale_data data;
    std::string line, logical_line;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        size_t bs = 0;
        for (auto it = line.rbegin(); it != line.rend() && *it == '\\'; ++it)
            ++bs;
        bool cont = (bs % 2 == 1);
        if (cont) line.pop_back();

        logical_line.empty() ? logical_line = line
                             : logical_line += line.substr(line.find_first_not_of(" \t\f"));
        if (cont) continue;

        if (logical_line.empty() || logical_line[0] == '#' || logical_line[0] == '!')
            { logical_line.clear(); continue; }

        size_t sep = std::string::npos;
        for (size_t i = 0; i < logical_line.size(); ++i) {
            if (logical_line[i] == '\\') { ++i; continue; }
            if (logical_line[i] == '=' || logical_line[i] == ':') { sep = i; break; }
        }
        if (sep == std::string::npos)
            { logical_line.clear(); continue; }

        std::string key = logical_line.substr(0, sep);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.pop_back();

        std::string val = logical_line.substr(sep + 1);
        size_t vs = val.find_first_not_of(" \t\f");
        val = (vs != std::string::npos) ? val.substr(vs) : "";

        data.strings[std::move(key)] = std::move(val);
        logical_line.clear();
    }

    translations_[locale] = std::move(data);

    console::info("i18n", "loaded %zu strings for locale '%s'",
                  translations_[locale].strings.size(), locale.c_str());
    return true;
}

void i18n_manager::set_locale(const std::string& locale) {
    current_locale_ = locale;
    console::info("i18n", "switched to locale '%s'", locale.c_str());
}

const std::string& i18n_manager::get_locale() const {
    return current_locale_;
}

std::string i18n_manager::tr(const std::string& key, const std::string& default_value) const {
    auto it = translations_.find(current_locale_);
    if (it != translations_.end()) {
        auto vit = it->second.strings.find(key);
        if (vit != it->second.strings.end())
            return vit->second;
    }

    if (current_locale_ != "zh-CN") {
        auto fallback = translations_.find("zh-CN");
        if (fallback != translations_.end()) {
            auto vit = fallback->second.strings.find(key);
            if (vit != fallback->second.strings.end())
                return vit->second;
        }
    }

    return default_value.empty() ? key : default_value;
}

void i18n_manager::clear() {
    translations_.clear();
    current_locale_ = "en-US";
}

std::vector<std::string> i18n_manager::available_locales() const {
    std::vector<std::string> locales;
    for (const auto& pair : translations_) {
        locales.push_back(pair.first);
    }
    return locales;
}

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
                     const std::string& default_value) const {
    std::string translated = tr(key, default_value);
    if (translated == key && default_value.empty()) {
        return translated;
    }
    return format_string(translated, args);
}

}
