/**
 * @file main.cpp
 * @brief 应用入口。
 * @author clk
 */

#include <application.h>
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

void write_crash_log(const EXCEPTION_RECORD* rec, DWORD code, PVOID* frames, WORD frame_count,
                     const CONTEXT* ctx) {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    char* slash = std::strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    std::strncat(path, "crash.log", MAX_PATH - std::strlen(path) - 1);

    char buf[4096];
    int len = 0;
    len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                         "=== crash @ %s ===\r\n", __DATE__);
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
    for (WORD i = 0; i < frame_count && len < static_cast<int>(sizeof(buf)) - 128; ++i) {
        len += std::snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                             "  #%u 0x%p\r\n", i, frames[i]);
    }

    HANDLE f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    SetProcessDPIAware();
    SetUnhandledExceptionFilter(crash_handler);

    auto instance = spiration::application::instance();
    instance->initialize();
    instance->loop();
    instance->shutdown();
    
    return 0;
}