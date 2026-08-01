/**
 * @file terminal_tool.cpp
 * @brief Windows 终端会话实现（cmd.exe 交互式进程 + 管道读写 + 后台读取线程）。
 * @author clk
 */

#include <extension/builtin/agent/tool/terminal_tool.h>

#include <windows.h>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace spiration {
namespace agent {

namespace {

// 简单校验字符串是否为合法 UTF-8
bool is_valid_utf8(const std::string& s) {
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { ++i; continue; }

        int len;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        else return false;

        if (i + len > n) return false;
        for (int k = 1; k < len; ++k) {
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80)
                return false;
        }
        unsigned char c0 = static_cast<unsigned char>(s[i]);
        if (len == 2 && c0 < 0xC2) return false;
        if (len == 3 && c0 == 0xE0 && (static_cast<unsigned char>(s[i + 1]) & 0xE0) == 0x80) return false;
        if (len == 4 && c0 == 0xF0 && (static_cast<unsigned char>(s[i + 1]) & 0xF0) == 0x80) return false;
        i += len;
    }
    return true;
}

// 本地代码页（OEM/ANSI）→ UTF-8；已是合法 UTF-8 则原样返回
std::string to_utf8(const std::string& input) {
    if (is_valid_utf8(input)) return input;
    for (UINT cp : {CP_OEMCP, CP_ACP}) {
        int wlen = MultiByteToWideChar(cp, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
        if (wlen <= 0) continue;
        std::wstring wstr(static_cast<size_t>(wlen), L'\0');
        if (MultiByteToWideChar(cp, 0, input.data(), static_cast<int>(input.size()), &wstr[0], wlen) <= 0)
            continue;
        int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wlen, nullptr, 0, nullptr, nullptr);
        if (ulen <= 0) continue;
        std::string utf8(static_cast<size_t>(ulen), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wlen, &utf8[0], ulen, nullptr, nullptr);
        return utf8;
    }
    return input;
}

// UTF-8 → OEM 代码页（写入 cmd.exe stdin 前转换，保证中文正确）
std::string to_oem(const std::string& input) {
    if (input.empty()) return input;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (wlen <= 0) return input;
    std::wstring wstr(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), &wstr[0], wlen);
    int olen = WideCharToMultiByte(CP_OEMCP, 0, wstr.data(), wlen, nullptr, 0, nullptr, nullptr);
    if (olen <= 0) return input;
    std::string out(static_cast<size_t>(olen), '\0');
    WideCharToMultiByte(CP_OEMCP, 0, wstr.data(), wlen, &out[0], olen, nullptr, nullptr);
    return out;
}

class win_terminal_session : public terminal_session {
public:
    explicit win_terminal_session(const std::string& shell_path);
    ~win_terminal_session() override;

    bool is_alive() const override { return alive_.load(); }
    void write(const std::string& data) override;
    size_t line_count() const override;
    std::vector<std::string> read_window(size_t from_bottom, size_t to_bottom) const override;
    std::string error() const override { return err_; }

private:
    HANDLE child_stdin_ = INVALID_HANDLE_VALUE;   // 写子进程 stdin
    HANDLE child_stdout_ = INVALID_HANDLE_VALUE;  // 读子进程 stdout
    HANDLE proc_ = nullptr;
    std::thread reader_;
    std::atomic<bool> alive_{false};
    std::string err_;

    mutable std::mutex mtx_;
    std::deque<std::string> lines_;
    std::string partial_;
    static constexpr size_t MAX_LINES = 5000;

    void reader_loop();
};

win_terminal_session::win_terminal_session(const std::string& shell_path) {
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    HANDLE inRead = INVALID_HANDLE_VALUE, inWrite = INVALID_HANDLE_VALUE;
    HANDLE outRead = INVALID_HANDLE_VALUE, outWrite = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&inRead, &inWrite, &sa, 0)) {
        err_ = "failed to create stdin pipe";
        return;
    }
    if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
        CloseHandle(inRead);
        CloseHandle(inWrite);
        err_ = "failed to create stdout pipe";
        return;
    }
    // 子进程继承 stdin 读端 / stdout 写端；父进程端不继承
    SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = inRead;
    si.hStdOutput = outWrite;
    si.hStdError = outWrite;

    std::string exe = shell_path.empty() ? "cmd.exe" : shell_path;
    std::string cmdline = "\"" + exe + "\"";

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(inRead);
    CloseHandle(outWrite);

    if (!ok) {
        CloseHandle(inWrite);
        CloseHandle(outRead);
        err_ = "failed to create process: " + exe;
        return;
    }

    child_stdin_ = inWrite;
    child_stdout_ = outRead;
    proc_ = pi.hProcess;
    CloseHandle(pi.hThread);
    alive_ = true;
    reader_ = std::thread(&win_terminal_session::reader_loop, this);
}

win_terminal_session::~win_terminal_session() {
    alive_ = false;
    if (proc_) {
        TerminateProcess(proc_, 0);
        WaitForSingleObject(proc_, 2000);
        CloseHandle(proc_);
        proc_ = nullptr;
    }
    if (child_stdin_ != INVALID_HANDLE_VALUE) {
        CloseHandle(child_stdin_);
        child_stdin_ = INVALID_HANDLE_VALUE;
    }
    if (child_stdout_ != INVALID_HANDLE_VALUE) {
        CloseHandle(child_stdout_);
        child_stdout_ = INVALID_HANDLE_VALUE;
    }
    if (reader_.joinable()) reader_.join();
}

void win_terminal_session::write(const std::string& data) {
    if (child_stdin_ == INVALID_HANDLE_VALUE) return;
    std::string oem = to_oem(data);
    DWORD written = 0;
    WriteFile(child_stdin_, oem.data(), static_cast<DWORD>(oem.size()), &written, nullptr);
}

size_t win_terminal_session::line_count() const {
    std::lock_guard<std::mutex> lk(mtx_);
    size_t n = lines_.size();
    if (!partial_.empty()) ++n;
    return n;
}

std::vector<std::string> win_terminal_session::read_window(size_t from_bottom,
                                                           size_t to_bottom) const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> all(lines_.begin(), lines_.end());
    if (!partial_.empty()) all.push_back(partial_);

    size_t total = all.size();
    if (total == 0) return {};
    if (from_bottom > to_bottom) std::swap(from_bottom, to_bottom);
    if (from_bottom < 1) from_bottom = 1;
    if (to_bottom > total) to_bottom = total;
    // 自下而上：绝对起点 = total - to_bottom，绝对终点(含) = total - from_bottom
    size_t start = total - to_bottom;
    size_t end = total - from_bottom;
    std::vector<std::string> out;
    out.reserve(end - start + 1);
    for (size_t i = start; i <= end; ++i) out.push_back(all[i]);
    return out;
}

void win_terminal_session::reader_loop() {
    char buf[4096];
    DWORD n = 0;
    while (alive_) {
        BOOL ok = ReadFile(child_stdout_, buf, sizeof(buf), &n, nullptr);
        if (!ok || n == 0) break;
        std::string chunk(buf, n);
        std::string utf8 = to_utf8(chunk);
        std::lock_guard<std::mutex> lk(mtx_);
        partial_ += utf8;
        size_t pos;
        while ((pos = partial_.find('\n')) != std::string::npos) {
            std::string line = partial_.substr(0, pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines_.push_back(line);
            partial_.erase(0, pos + 1);
            if (lines_.size() > MAX_LINES) lines_.pop_front();
        }
    }
    alive_ = false;
}

} // namespace

std::unique_ptr<terminal_session> create_terminal_session(const std::string& shell_path) {
    return std::make_unique<win_terminal_session>(shell_path);
}

} // namespace agent
} // namespace spiration
