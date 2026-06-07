/**
 * @file text_document.h
 * @brief 高性能文本缓冲区，支持大文件（10M+）的快速行查找与编辑。
 * @author clk
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace spiration {

/**
 * @brief 文本缓冲区，使用行索引加速行级操作。
 *
 * 优化策略：
 * - 行偏移索引：O(1) 行号 ↔ 行内容
 * - 惰性重建：仅在内容变更后重建索引
 * - 虚拟行号：起始行号可偏移（用于片段视图）
 */
class text_document {
public:
    text_document() = default;

    
    void set_text(const std::string& text);
    const std::string& text() const { return buffer_; }

    
    bool load_file(const std::string& path);

    
    size_t line_count() const;

    
    const std::string& line(size_t n) const;

    
    size_t line_offset(size_t n) const;

    
    void insert(size_t line, size_t col, const std::string& text);

    
    void erase(size_t line, size_t col, size_t end_line, size_t end_col);

    
    size_t char_count() const { return buffer_.size(); }

    
    void mark_dirty() { lines_dirty_ = true; }

private:
    std::string buffer_;
    mutable std::vector<size_t> line_offsets_;
    mutable std::vector<std::string> line_cache_;
    mutable bool lines_dirty_ = false;

    void rebuild_lines() const;
};

} 
