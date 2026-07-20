/**
 * @file extension_manifest.cpp
 * @brief extension.json 解析器实现。
 * @author clk
 */

#include <extension/extension_manifest.h>
#include <utils/console.h>

#include <cctype>
#include <cstdlib>

namespace spiration {

namespace {

class json_parser {
public:
    explicit json_parser(const std::string& src) : src_(src), pos_(0) {}

    std::optional<manifest_data> parse() {
        skip_ws();
        if (!expect('{')) return {};
        auto obj = parse_object();
        if (!obj) return {};

        auto& obj_ref = *obj;

        manifest_data m;
        m.id              = get_string(&obj_ref, "id");
        m.name            = get_string(&obj_ref, "name");
        m.version         = get_string(&obj_ref, "version");
        m.description     = get_string(&obj_ref, "description");
        m.author          = get_string(&obj_ref, "author");
        m.main            = get_string(&obj_ref, "main");

        auto api_it = obj_ref.find("api_version");
        if (api_it != obj_ref.end() && api_it->second.type == json_type::number) {
            m.api_version = static_cast<int>(api_it->second.num_val);
        }

        auto dep_it = obj_ref.find("depends");
        if (dep_it != obj_ref.end() && dep_it->second.type == json_type::object) {
            for (const auto& [k, v] : dep_it->second.obj_val) {
                if (v.type == json_type::string) {
                    m.depends[k] = v.str_val;
                }
            }
        }

        if (m.id.empty() || m.name.empty() || m.version.empty() || m.main.empty()) {
            console::error("extension_manifest: missing required fields (id/name/version/main)");
            return {};
        }

        return m;
    }

private:
    enum class json_type { null, boolean, number, string, array, object };

    struct json_value {
        json_type type = json_type::null;
        std::string str_val;
        double num_val = 0;
        bool bool_val = false;
        std::vector<json_value> arr_val;
        std::map<std::string, json_value> obj_val;
    };

    static std::string get_string(const std::map<std::string, json_value>* obj,
                                  const std::string& key) {
        if (!obj) return {};
        auto it = obj->find(key);
        if (it != obj->end() && it->second.type == json_type::string) {
            return it->second.str_val;
        }
        return {};
    }

    void skip_ws() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
    }

    bool expect(char c) {
        skip_ws();
        if (pos_ >= src_.size() || src_[pos_] != c) return false;
        ++pos_;
        return true;
    }

    char peek() {
        skip_ws();
        if (pos_ >= src_.size()) return '\0';
        return src_[pos_];
    }

    std::string parse_string() {
        if (!expect('"')) return {};
        std::string result;
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '"') return result;
            if (c == '\\' && pos_ < src_.size()) {
                char esc = src_[pos_++];
                switch (esc) {
                    case '"':  result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    default:   result += esc; break;
                }
            } else {
                result += c;
            }
        }
        return result;
    }

    json_value parse_value() {
        skip_ws();
        char c = peek();
        if (c == '"') {
            json_value v;
            v.type = json_type::string;
            v.str_val = parse_string();
            return v;
        }
        if (c == '{') {
            auto obj = parse_object();
            if (obj) {
                json_value v;
                v.type = json_type::object;
                v.obj_val = std::move(*obj);
                return v;
            }
            return {};
        }
        if (c == '[') {
            ++pos_; // '['
            json_value v;
            v.type = json_type::array;
            skip_ws();
            if (peek() != ']') {
                while (true) {
                    auto elem = parse_value();
                    v.arr_val.push_back(std::move(elem));
                    skip_ws();
                    if (peek() == ',') { ++pos_; skip_ws(); }
                    else break;
                }
            }
            expect(']');
            return v;
        }
        // number
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            json_value v;
            v.type = json_type::number;
            char* end = nullptr;
            v.num_val = std::strtod(src_.c_str() + pos_, &end);
            pos_ = static_cast<size_t>(end - src_.c_str());
            return v;
        }
        // literal: true/false/null
        if (src_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            json_value v; v.type = json_type::boolean; v.bool_val = true;
            return v;
        }
        if (src_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            json_value v; v.type = json_type::boolean; v.bool_val = false;
            return v;
        }
        if (src_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            json_value v; v.type = json_type::null;
            return v;
        }
        return {};
    }

    std::optional<std::map<std::string, json_value>> parse_object() {
        if (!expect('{')) return {};
        std::map<std::string, json_value> result;
        skip_ws();
        if (peek() == '}') { ++pos_; return result; }
        while (true) {
            skip_ws();
            std::string key = parse_string();
            if (key.empty()) return {};
            if (!expect(':')) return {};
            json_value val = parse_value();
            result[key] = std::move(val);
            skip_ws();
            if (peek() == ',') { ++pos_; skip_ws(); }
            else break;
        }
        expect('}');
        return result;
    }

    const std::string& src_;
    size_t pos_;
};

} // anonymous namespace

std::optional<manifest_data> parse_extension_manifest(const std::string& json) {
    json_parser parser(json);
    return parser.parse();
}

} // namespace spiration
