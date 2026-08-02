/**
 * @file clipboard.cpp
 * @brief 剪贴板实现。
 * @author clk
 */

#include <utils/clipboard.h>

#include <cstdio>
#include <string>

namespace spiration {

void clipboard::copy(const std::string& text) {
    if (text.empty()) return;
    FILE* pipe = popen("pbcopy", "w");
    if (pipe) {
        fwrite(text.data(), 1, text.size(), pipe);
        pclose(pipe);
    }
}

std::string clipboard::paste() {
    FILE* pipe = popen("pbpaste", "r");
    if (pipe) {
        std::string out;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) out.append(buf, n);
        pclose(pipe);
        return out;
    }
    return {};
}

} // namespace spiration
