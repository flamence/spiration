/**
 * @file syntax_highlighter.cpp
 * @brief 代码语法高亮器实现。
 * @author clk
 */

#include <utils/syntax_highlighter.h>
#include <cctype>

namespace spiration {

syntax_highlighter::syntax_highlighter() {
    build_keywords();
}

void syntax_highlighter::set_language(const std::string& lang) {
    language_ = lang;
    build_keywords();
}

void syntax_highlighter::build_keywords() {
    keywords_ = {
        "auto", "break", "case", "catch", "class", "const", "constexpr",
        "continue", "decltype", "default", "delete", "do", "else", "enum",
        "explicit", "export", "extern", "false", "for", "friend", "goto",
        "if", "inline", "mutable", "namespace", "new", "noexcept", "nullptr",
        "operator", "override", "private", "protected", "public",
        "register", "return", "requires", "sizeof", "static",
        "struct", "switch", "template", "this", "throw", "true", "try",
        "typedef", "typeid", "typename", "union", "using", "virtual",
        "void", "volatile", "while", "include", "define", "ifdef",
        "ifndef", "endif", "pragma", "import", "module",
        "int", "float", "double", "char", "bool", "long", "short",
        "unsigned", "signed", "size_t", "uint32_t", "int32_t",
        "uint64_t", "int64_t", "uint8_t", "int8_t", "std",
        "string", "vector", "map", "unique_ptr", "shared_ptr",
        "nullptr", "true", "false", "auto",
    };
}

void syntax_highlighter::invalidate_line(size_t n) {
    if (n < line_valid_.size()) line_valid_[n] = false;
}

void syntax_highlighter::invalidate_all() {
    line_valid_.clear();
    line_cache_.clear();
}

const std::vector<syntax_token>& syntax_highlighter::highlight_line(size_t n, const std::string& line_text) {
    if (n >= line_cache_.size()) {
        line_cache_.resize(n + 64);
        line_valid_.resize(n + 64, false);
    }
    if (!line_valid_[n]) {
        line_cache_[n].clear();
        tokenize_line(n, line_text);
        line_valid_[n] = true;
    }
    return line_cache_[n];
}

void syntax_highlighter::tokenize_line(size_t n, const std::string& text) {
    auto& tokens = line_cache_[n];
    if (text.empty()) return;

    enum State { normal, comment, preprocessor };
    State state = normal;
    size_t i = 0;

    while (i < text.size()) {
        
        if (i == 0 && text[i] == '#') {
            size_t start = i;
            while (i < text.size() && text[i] != '\n' && text[i] != '\r') i++;
            tokens.push_back({start, i - start, preprocessor_color()});
            continue;
        }

        
        if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
            size_t start = i;
            i = text.size();
            tokens.push_back({start, i - start, comment_color()});
            continue;
        }

        
        if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '*') {
            size_t start = i;
            i += 2;
            while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) i++;
            if (i + 1 < text.size()) i += 2;
            tokens.push_back({start, i - start, comment_color()});
            continue;
        }

        
        if (text[i] == '"') {
            size_t start = i;
            i++;
            while (i < text.size() && text[i] != '"') {
                if (text[i] == '\\') i++;
                i++;
            }
            if (i < text.size()) i++;
            tokens.push_back({start, i - start, string_color()});
            continue;
        }

        
        if (text[i] == '\'') {
            size_t start = i;
            i++;
            while (i < text.size() && text[i] != '\'') {
                if (text[i] == '\\') i++;
                i++;
            }
            if (i < text.size()) i++;
            tokens.push_back({start, i - start, string_color()});
            continue;
        }

        
        if (std::isdigit(static_cast<unsigned char>(text[i]))) {
            size_t start = i;
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '.' || text[i] == 'x' || text[i] == 'X')) i++;
            tokens.push_back({start, i - start, number_color()});
            continue;
        }

        
        if (std::isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_') {
            size_t start = i;
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) i++;
            std::string word = text.substr(start, i - start);
            color c = keywords_.count(word) ? keyword_color() : default_color();
            tokens.push_back({start, i - start, c});
            continue;
        }

        
        size_t start = i;
        while (i < text.size() &&
               !(text[i] == '"' || text[i] == '\'' ||
                 std::isdigit(static_cast<unsigned char>(text[i])) ||
                 std::isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_' ||
                 (i + 1 < text.size() && text[i] == '/' && (text[i + 1] == '/' || text[i + 1] == '*')))) {
            i++;
        }
        tokens.push_back({start, i - start, default_color()});
    }

    if (tokens.empty()) {
        tokens.push_back({0, text.size(), default_color()});
    }
}

} 
