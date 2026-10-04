/**
 * @file file.cpp
 * @brief 文件接收器实现。
 * @author clk
 */

#include <spiration/logging/file_sink.h>
#include <spiration/logging/log_formatter.hpp>

#include <cstdio>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace spiration {
namespace logging {

namespace {

#if defined(_WIN32)

/**
 * @brief 由 UTF-8 路径取得宽字符路径。
 * @return 成功返回 `true`；失败时 `out` 保持不变。
 */
bool to_wide_path(const ghc::filesystem::path& path, std::wstring* out) {
    const std::string utf8 = path.u8string();
    const int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                           static_cast<int>(utf8.size()),
                                           nullptr, 0);
    if (size <= 0) return false;
    out->assign(static_cast<size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                          static_cast<int>(utf8.size()), &(*out)[0], size);
    return true;
}

#endif // defined(_WIN32)

} // namespace

file_sink::file_sink(const ghc::filesystem::path& path) : path_(path) {
#if defined(_WIN32)
    std::wstring wide;
    if (to_wide_path(path, &wide)) {
        file_.open(wide.c_str(), std::ios::app);
    } else {
        file_.open(path.string(), std::ios::app);
    }
#else
    file_.open(path.string(), std::ios::app);
#endif

    if (!file_.is_open()) {
        report("cannot open log file");
    }
}

file_sink::~file_sink() {
    if (file_.is_open()) file_.close();
}

void file_sink::report(const char* reason) {
    if (reported_) return;
    reported_ = true;
    std::fprintf(stderr, "[logging] %s: %s\n", reason, path_.string().c_str());

#if defined(_WIN32)
    {
        wchar_t appdata[MAX_PATH] = {0};
        const DWORD n = ::GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
        char utf8[MAX_PATH * 4] = {0};
        if (n > 0) {
            ::WideCharToMultiByte(CP_UTF8, 0, appdata, static_cast<int>(n),
                                  utf8, sizeof(utf8) - 1, nullptr, nullptr);
        }
        std::fprintf(stderr, "[logging]   APPDATA   = %s\n",
                     n > 0 ? utf8 : "(unset)");
        std::fprintf(stderr, "[logging]   parent exists = %d\n",
                     (int)ghc::filesystem::exists(path_.parent_path()));
        std::fprintf(stderr, "[logging]   is directory  = %d\n",
                     (int)ghc::filesystem::is_directory(path_.parent_path()));
    }
#endif

    std::fflush(stderr);
}

void file_sink::write(const log& message, log_formatter formatter) {
    if (!file_.is_open()) {
        report("log file is not writable, messages are dropped");
        return;
    }

    file_ << formatter.format(message);
    file_.flush();

    if (file_.bad()) {
        report("write failed");
    }
}

}
}
