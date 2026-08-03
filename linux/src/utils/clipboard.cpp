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

void (*g_copy_fn)(const std::string&) = nullptr;
std::string (*g_paste_fn)() = nullptr;

} // namespace

void x11_clipboard_bind(void (*copy_fn)(const std::string&), std::string (*paste_fn)()) {
    g_copy_fn = copy_fn;
    g_paste_fn = paste_fn;
}

void clipboard::copy(const std::string& text) {
    if (text.empty()) return;
    if (g_copy_fn) {
        g_copy_fn(text);
        return;
    }
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
    if (g_paste_fn) return g_paste_fn();
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
