/**
 * @file properties.cpp
 * @author clk
 * @brief 属性工具实现。
 */

#include <tl/optional.hpp>
#include <spiration/utils/properties.h>

#include <cstdint>
#include <fstream>

namespace spiration {
namespace {

inline bool is_blank(char c) {
    return c == ' ' || c == '\t' || c == '\f';
}

inline bool is_hex(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

inline unsigned hex_value(char c) {
    if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
    return static_cast<unsigned>(c - 'A' + 10);
}

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::size_t escape_length(const std::string& s, std::size_t i) {
    if (i + 1 >= s.size()) return 1;
    if (s[i + 1] == 'u' && i + 5 < s.size()) return 6;
    return 2;
}

std::string decode_escapes(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    const std::size_t n = s.size();
    while (i < n) {
        if (s[i] != '\\') {
            out.push_back(s[i++]);
            continue;
        }
        if (i + 1 >= n) {
            out.push_back('\\');
            ++i;
            continue;
        }
        const char e = s[i + 1];
        switch (e) {
            case 't': out.push_back('\t'); i += 2; break;
            case 'n': out.push_back('\n'); i += 2; break;
            case 'r': out.push_back('\r'); i += 2; break;
            case 'f': out.push_back('\f'); i += 2; break;
            case 'u': {
                if (i + 5 < n && is_hex(s[i + 2]) && is_hex(s[i + 3]) &&
                    is_hex(s[i + 4]) && is_hex(s[i + 5])) {
                    std::uint32_t cp = (hex_value(s[i + 2]) << 12) |
                                       (hex_value(s[i + 3]) << 8) |
                                       (hex_value(s[i + 4]) << 4) |
                                       hex_value(s[i + 5]);
                    i += 6;
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 5 < n &&
                        s[i] == '\\' && s[i + 1] == 'u' &&
                        is_hex(s[i + 2]) && is_hex(s[i + 3]) &&
                        is_hex(s[i + 4]) && is_hex(s[i + 5])) {
                        std::uint32_t lo = (hex_value(s[i + 2]) << 12) |
                                           (hex_value(s[i + 3]) << 8) |
                                           (hex_value(s[i + 4]) << 4) |
                                           hex_value(s[i + 5]);
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            i += 6;
                        }
                    }
                    append_utf8(out, cp);
                } else {
                    out.push_back('u');
                    i += 2;
                }
                break;
            }
            default:
                out.push_back(e);
                i += 2;
                break;
        }
    }
    return out;
}

void store_entry(std::unordered_map<std::string, std::string>& data,
                 const std::string& entry) {
    std::size_t i = 0;
    const std::size_t n = entry.size();
    std::string key;

    while (i < n) {
        const char c = entry[i];
        if (c == '\\') {
            const std::size_t len = escape_length(entry, i);
            key.append(entry, i, len);
            i += len;
            continue;
        }
        if (c == '=' || c == ':') {
            i++;
            break;
        }
        if (is_blank(c)) {
            while (i < n && is_blank(entry[i])) ++i;
            if (i < n && (entry[i] == '=' || entry[i] == ':')) ++i;
            break;
        }
        key.push_back(c);
        i++;
    }

    while (i < n && is_blank(entry[i])) ++i;
    const std::string value = decode_escapes(entry.substr(i));

    data[decode_escapes(key)] = value;
}

} // namespace

bool properties::load(const ghc::filesystem::path& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::string text;
    std::string line;
    while (std::getline(file, line)) {
        text += line;
        text.push_back('\n');
    }
    parse(text);
    return true;
}

void properties::parse(const std::string& text) {
    data_.clear();

    std::string logical_line;
    std::string::size_type pos = 0;
    const std::string::size_type len = text.size();

    while (pos < len) {
        std::string::size_type eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = len;
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::size_t bs = 0;
        for (auto it = line.rbegin(); it != line.rend() && *it == '\\'; ++it)
            ++bs;
        const bool continued = (bs % 2 == 1);
        if (continued)
            line.pop_back();

        if (logical_line.empty()) {
            const std::string::size_type s = line.find_first_not_of(" \t\f");
            if (s == std::string::npos)
                continue;
            if (line[s] == '#' || line[s] == '!')
                continue;
            logical_line = line.substr(s);
        } else {
            const std::string::size_type s = line.find_first_not_of(" \t\f");
            logical_line += (s == std::string::npos) ? std::string() : line.substr(s);
        }

        if (continued)
            continue;

        store_entry(data_, logical_line);
        logical_line.clear();
    }

    if (!logical_line.empty())
        store_entry(data_, logical_line);
}

bool properties::contains(const std::string& key) const {
    return data_.find(key) != data_.end();
}

tl::optional<std::string> properties::get(const std::string& key) const {
    auto it = data_.find(key);
    return it == data_.end() ? tl::nullopt : tl::optional<std::string>(it->second);
}

const std::unordered_map<std::string, std::string>& properties::entries() const {
    return data_;
}

std::size_t properties::size() const {
    return data_.size();
}

} // namespace spiration
