/**
 * @file crash_log.cpp
 * @brief 崩溃日志实现。
 * @author clk
 */

#include <utils/crash_log.h>

#if defined(_WIN32)

#include <cstdio>
#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>

namespace spiration {
namespace crash_log {

namespace {

char g_path[2048] = {0};

void symbolize_address(void* addr, char* out, size_t out_sz) {
    out[0] = '\0';
    static bool inited = [] {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        return SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    }();
    if (!inited) {
        std::snprintf(out, out_sz, "0x%p", addr);
        return;
    }
    alignas(SYMBOL_INFO) char sym_buf[sizeof(SYMBOL_INFO) + 256];
    auto* sym = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    DWORD64 disp = 0;
    if (SymFromAddr(GetCurrentProcess(), reinterpret_cast<DWORD64>(addr), &disp, sym)) {
        std::snprintf(out, out_sz, "%s+0x%llX", sym->Name,
                      static_cast<unsigned long long>(disp));
    } else {
        std::snprintf(out, out_sz, "0x%p", addr);
    }
}

void write_crash_log(const EXCEPTION_RECORD* rec, DWORD code, PVOID* frames,
                     WORD frame_count, const CONTEXT* ctx) {
    if (g_path[0] == '\0') return;
    char buf[4096];
    int len = 0;
    len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                         "\n=== crash @ %s ===\r\n", __DATE__);
    len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                         "exception code : 0x%08lX\r\n", code);
    len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                         "module base    : 0x%p\r\n", GetModuleHandleA(nullptr));
    if (rec) {
        len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                             "address        : 0x%p\r\n", rec->ExceptionAddress);
    }
    if (ctx) {
        len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                             "fault rip      : 0x%p\r\n", reinterpret_cast<void*>(ctx->Rip));
    }
    len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                         "stack trace    :\r\n");
    char sym[512];
    for (WORD i = 0; i < frame_count && len < static_cast<int>(sizeof(buf)) - 640; ++i) {
        symbolize_address(frames[i], sym, sizeof(sym));
        len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                             "  #%u %s\r\n", i, sym);
    }

    HANDLE f = CreateFileA(g_path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(f, buf, static_cast<DWORD>(len), &written, nullptr);
        CloseHandle(f);
    }
}

LONG WINAPI crash_handler(EXCEPTION_POINTERS* ep) {
    PVOID frames[32];
    WORD count = RtlCaptureStackBackTrace(0, 32, frames, nullptr);
    write_crash_log(ep ? ep->ExceptionRecord : nullptr,
                    ep ? ep->ExceptionRecord->ExceptionCode : 0xC0000005L,
                    frames, count,
                    ep ? ep->ContextRecord : nullptr);
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void install(const std::string& path) {
    if (path.empty()) return;
    std::strncpy(g_path, path.c_str(), sizeof(g_path) - 1);
    g_path[sizeof(g_path) - 1] = '\0';
    SetUnhandledExceptionFilter(crash_handler);
}

} // namespace crash_log
} // namespace spiration
#else

#include <cstdio>
#include <csignal>
#include <cstring>
#include <ctime>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <execinfo.h>

namespace spiration {
namespace crash_log {

namespace {

char g_path[2048] = {0};

void write_str(int fd, const char* s) {
    if (!s || *s == '\0') return;
    ::write(fd, s, static_cast<size_t>(std::strlen(s)));
}

void crash_handler(int sig) {
    char buf[512];
    int len = 0;

    std::time_t now = std::time(nullptr);
    struct tm tmv;
    ::localtime_r(&now, &tmv);
    char ts[64];
    ::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                         "=== crash @ %s ===\n", ts);
    len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                         "signal         : %d (%s)\n", sig, ::strsignal(sig));
    len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                         "stack trace    :\n");

    int fd = ::open(g_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    ::write(fd, buf, static_cast<size_t>(len));

    void* frames[64];
    int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, fd);

    ::write(fd, "\n", 1);
    ::close(fd);

    std::signal(sig, SIG_DFL);
    ::raise(sig);
}

} // namespace

void install(const std::string& path) {
    if (path.empty()) return;
    std::strncpy(g_path, path.c_str(), sizeof(g_path) - 1);
    g_path[sizeof(g_path) - 1] = '\0';

    auto pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0) {
        ::mkdir(path.substr(0, pos).c_str(), 0755);
    }

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    for (int s : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS}) {
        ::sigaction(s, &sa, nullptr);
    }
}

} // namespace crash_log
} // namespace spiration
#endif
