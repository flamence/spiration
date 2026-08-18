#include <spiration/application.h>
#include <spiration/extension/builtin/i18n/i18n.h>
#include <spiration/utils/properties.h>
#include <algorithm>
#include <cctype>
#include <fmt/format.h>

namespace spiration {

i18n_manager& i18n_manager::get() {
    static i18n_manager instance;
    return instance;
}

bool i18n_manager::load(const std::string& locale, const ghc::filesystem::path& filepath) {
    properties props;
    if (!props.load(filepath)) {
        application::instance()->logger()->log({logging::level::warning, fmt::format("cannot open '{}'", filepath.string()), "i18n"});
        return false;
    }

    locale_data data;
    data.strings = props.entries();

    translations_[locale] = std::move(data);

    application::instance()->logger()->log({logging::level::info, fmt::format("loaded {} strings for locale '{}'", translations_[locale].strings.size(), locale), "i18n"});
    return true;
}

void i18n_manager::set_locale(const std::string& locale) {
    current_locale_ = locale;
    application::instance()->logger()->log({logging::level::info, fmt::format("switched to locale '{}'", locale), "i18n"});
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
