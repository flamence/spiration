/**
 * @file crash_log.cpp
 * @brief 崩溃日志实现。
 * @author clk
 */

#include <utils/crash_log.h>

#if defined(_WIN32)
namespace spiration {
namespace crash_log {
void install(const std::string&) {}
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

#if !defined(OHOS_PLATFORM)
#include <execinfo.h>
#endif

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

#if defined(OHOS_PLATFORM)
    for (int i = 0; i < 32; ++i) {
        void* ra = __builtin_return_address(i);
        if (!ra) break;
        char line[96];
        int n = std::snprintf(line, sizeof(line), "  #%d 0x%p\n", i, ra);
        ::write(fd, line, static_cast<size_t>(n));
    }
#else
    void* frames[64];
    int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, fd);
#endif

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
