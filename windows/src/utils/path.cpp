/**
 * @file path.cpp
 * @brief 路径实现。
 * @author clk
 */

#include <utils/path.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace spiration {

std::filesystem::path path::u8path(const std::string& s) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (wlen <= 1) return std::filesystem::path();
    std::wstring w(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], wlen);
    return std::filesystem::path(w);
}

} // namespace spiration
