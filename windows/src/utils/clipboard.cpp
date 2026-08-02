/**
 * @file clipboard.cpp
 * @brief 平台剪贴板实现。
 * @author clk
 */

#include <utils/clipboard.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace spiration {

void clipboard::copy(const std::string& text) {
    if (text.empty()) return;
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
}

std::string clipboard::paste() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return {};
    if (!OpenClipboard(nullptr)) return {};
    std::string result;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        wchar_t* src = static_cast<wchar_t*>(GlobalLock(h));
        if (src) {
            int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, nullptr, 0,
                                         nullptr, nullptr);
            if (len > 1) {
                result.resize(static_cast<size_t>(len) - 1);
                WideCharToMultiByte(CP_UTF8, 0, src, -1, result.data(), len,
                                    nullptr, nullptr);
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return result;
}

} // namespace spiration
