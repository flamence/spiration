/**
 * @file clipboard.cpp
 * @brief 剪贴板实现。
 * @author clk
 */

#include <utils/clipboard.h>
#include <utils/console.h>

#include <cstdio>
#include <string>

namespace spiration {

namespace {

bool tool_available(const char* tool) {
    std::string cmd = std::string("command -v ") + tool + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

} // namespace

void clipboard::copy(const std::string& text) {
    if (text.empty()) return;
    if (!tool_available("xclip")) {
        console::warning("clipboard",
                         "xclip not found; install with: sudo apt install xclip");
        return;
    }
    FILE* pipe = popen("xclip -selection clipboard -i >/dev/null 2>&1", "w");
    if (pipe) {
        fwrite(text.data(), 1, text.size(), pipe);
        pclose(pipe);
    }
}

std::string clipboard::paste() {
    if (!tool_available("xclip")) {
        console::warning("clipboard",
                         "xclip not found; install with: sudo apt install xclip");
        return {};
    }
    FILE* pipe = popen("xclip -selection clipboard -o 2>/dev/null", "r");
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
