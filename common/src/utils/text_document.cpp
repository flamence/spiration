/**
 * @file text_document.cpp
 * @brief 高性能文本缓冲区实现。
 * @author clk
 */

#include <utils/text_document.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace spiration {

void text_document::set_text(const std::string& text) {
    buffer_ = text;
    lines_dirty_ = true;
}

bool text_document::load_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0);
    buffer_.resize(size);
    file.read(buffer_.data(), static_cast<std::streamsize>(size));
    file.close();
    
    size_t write = 0;
    bool prevCR = false;
    for (size_t i = 0; i < size; ++i) {
        char c = buffer_[i];
        if (c == '\r') { prevCR = true; continue; }
        if (prevCR) { if (c != '\n') buffer_[write++] = '\n'; prevCR = false; }
        buffer_[write++] = c;
    }
    if (prevCR) buffer_[write++] = '\n';
    buffer_.resize(write);
    lines_dirty_ = true;
    return true;
}

void text_document::rebuild_lines() const {
    line_offsets_.clear();
    line_cache_.clear();
    line_offsets_.push_back(0);
    for (size_t i = 0; i < buffer_.size(); ++i) {
        if (buffer_[i] == '\n') {
            line_offsets_.push_back(i + 1);
        }
    }
    if (line_offsets_.empty() || line_offsets_.back() <= buffer_.size()) {
        line_offsets_.push_back(buffer_.size());
    }
    lines_dirty_ = false;
}

size_t text_document::line_count() const {
    if (lines_dirty_) rebuild_lines();
    if (line_offsets_.size() <= 1) return buffer_.empty() ? 0 : 1;
    
    return line_offsets_.size() - 1;
}

size_t text_document::line_offset(size_t n) const {
    if (lines_dirty_) rebuild_lines();
    if (n >= line_offsets_.size()) n = line_offsets_.size() - 1;
    return line_offsets_[n];
}

const std::string& text_document::line(size_t n) const {
    if (lines_dirty_) rebuild_lines();
    if (n >= line_count()) { static std::string empty; return empty; }
    
    while (line_cache_.size() <= n) line_cache_.resize(n + 16);
    if (line_cache_[n].empty()) {
        
        size_t start = line_offsets_[n];
        size_t end = (n + 1 < line_offsets_.size()) ? line_offsets_[n + 1] : buffer_.size();
        
        if (end > start && buffer_[end - 1] == '\n') end--;
        if (end > start && buffer_[end - 1] == '\r') end--;
        line_cache_[n].assign(buffer_.data() + start, end - start);
    }
    return line_cache_[n];
}

void text_document::insert(size_t line, size_t col, const std::string& text) {
    size_t pos = line_offset(line) + col;
    buffer_.insert(pos, text);
    lines_dirty_ = true;
}

void text_document::erase(size_t line, size_t col, size_t end_line, size_t end_col) {
    size_t start = line_offset(line) + col;
    size_t end = line_offset(end_line) + end_col;
    if (end > buffer_.size()) end = buffer_.size();
    buffer_.erase(start, end - start);
    lines_dirty_ = true;
}

} 
