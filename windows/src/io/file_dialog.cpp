/**
 * @file file_dialog.cpp
 * @brief 文件对话框实现。
 * @author clk
 */

#include <io/file_dialog.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>

/** @brief 将 UTF-8 字符串转换为宽字符串。 */
static std::wstring to_wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], len);
    return w;
}

/** @brief 将宽字符串转换为 UTF-8 字符串。 */
static std::string from_wide(const wchar_t* wide, int len) {
    if (len <= 0) return {};
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide, len, nullptr, 0, nullptr, nullptr);
    std::string result(utf8_len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, len, result.data(), utf8_len, nullptr, nullptr);
    return result;
}

/**
 * @brief 构造 Win32 OPENFILENAMEW 所需的过滤器宽字符串缓冲区。
 * @param desc 过滤器描述文本
 * @param patterns 文件扩展名模式列表
 * @return 双 null 结尾的宽字符过滤器缓冲区
 */
static std::vector<wchar_t> win32_filter_w(const std::string& desc,
                                            const std::vector<std::string>& patterns) {
    std::wstring w = to_wide(desc);
    w.push_back(L'\0');
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) w.push_back(L';');
        std::wstring wp = to_wide(patterns[i]);
        w += wp;
    }
    w.push_back(L'\0');
    w.push_back(L'\0');
    std::vector<wchar_t> buf(w.begin(), w.end());
    return buf;
}

namespace spiration {
namespace io {

std::string open_file(const std::string& title,
                      const std::string& filter,
                      const std::vector<std::string>& patterns) {
    std::vector<wchar_t> filter_w = win32_filter_w(filter, patterns);
    std::wstring title_w = to_wide(title);

    wchar_t path[MAX_PATH] = {0};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.lpstrFilter = filter_w.data();
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title_w.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        return from_wide(path, (int)wcslen(path));
    }
    return {};
}

std::string save_file(const std::string& title,
                      const std::string& filter,
                      const std::vector<std::string>& patterns) {
    std::vector<wchar_t> filter_w = win32_filter_w(filter, patterns);
    std::wstring title_w = to_wide(title);

    wchar_t path[MAX_PATH] = {0};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.lpstrFilter = filter_w.data();
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title_w.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn)) {
        return from_wide(path, (int)wcslen(path));
    }
    return {};
}

} // namespace io
} // namespace spiration