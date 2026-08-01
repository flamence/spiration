/**
 * @file clipboard.cpp
 * @brief 跨平台剪贴板工具实现。
 * @author clk
 */

#include <utils/clipboard.h>

#include <cstdio>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace spiration {

void clipboard::copy(const std::string& text) {
    if (text.empty()) return;

#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wlen) * sizeof(wchar_t));
    if (h) {
        wchar_t* dst = static_cast<wchar_t*>(GlobalLock(h));
        if (dst) {
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, dst, wlen);
            GlobalUnlock(h);
            if (!SetClipboardData(CF_UNICODETEXT, h))
                GlobalFree(h);
        } else {
            GlobalFree(h);
        }
    }
    CloseClipboard();
#elif defined(__APPLE__)
    FILE* pipe = popen("pbcopy", "w");
    if (pipe) {
        fwrite(text.data(), 1, text.size(), pipe);
        pclose(pipe);
    }
#elif defined(__linux__)
    FILE* pipe = popen("xclip -selection clipboard -i >/dev/null 2>&1", "w");
    if (pipe) {
        fwrite(text.data(), 1, text.size(), pipe);
        pclose(pipe);
    }
#endif
}

std::string clipboard::paste() {
#ifdef _WIN32
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return {};
    if (!OpenClipboard(nullptr)) return {};
    std::string result;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        wchar_t* src = static_cast<wchar_t*>(GlobalLock(h));
        if (src) {
            int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, nullptr, 0, nullptr, nullptr);
            if (len > 1) {
                result.resize(static_cast<size_t>(len) - 1);
                WideCharToMultiByte(CP_UTF8, 0, src, -1, result.data(), len, nullptr, nullptr);
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return result;
#elif defined(__APPLE__)
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
#elif defined(__linux__)
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
#else
    return {};
#endif
}

} // namespace spiration
