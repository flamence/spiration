/**
 * @file main.cpp
 * @brief 应用入口。
 * @author clk
 */

#include <application.h>
#include <utils/console.h>
#include <utils/platform.h>
#include <windows.h>
#include <dbghelp.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

char g_crash_log_path[MAX_PATH] = {0};

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

void write_crash_log(const EXCEPTION_RECORD* rec, DWORD code, PVOID* frames, WORD frame_count,
                     const CONTEXT* ctx) {
    if (g_crash_log_path[0] == '\0') return;
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

    HANDLE f = CreateFileA(g_crash_log_path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(f, buf, static_cast<DWORD>(len), &written, nullptr);
        CloseHandle(f);
    } else {
        fprintf(stderr, "[crash] open log failed: path='%s' err=%lu\n",
                g_crash_log_path, GetLastError());
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

void install_crash_handler() {
    std::string p = spiration::console::log_file_path();
    if (!p.empty()) {
        std::strncpy(g_crash_log_path, p.c_str(), MAX_PATH - 1);
        g_crash_log_path[MAX_PATH - 1] = '\0';
    }
    SetUnhandledExceptionFilter(crash_handler);
}

std::string executable_directory() {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return ".";
    char* slash = std::strrchr(path, '\\');
    if (slash) *slash = '\0';
    return std::string(path);
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    SetProcessDPIAware();
    std::string log_dir = spiration::platform::join_path(executable_directory(), "logs");
    spiration::platform::create_directory(log_dir);
    spiration::console::set_log_file(
        spiration::console::make_log_path(log_dir, "spiration"));
    install_crash_handler();

    auto instance = spiration::application::instance();
    instance->initialize();
    instance->loop();
    instance->shutdown();
    
    return 0;
}