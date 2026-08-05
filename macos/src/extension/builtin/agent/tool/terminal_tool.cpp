/**
 * @file terminal_tool.cpp
 * @brief 终端会话实现。
 * @author clk
 */

#include <extension/builtin/agent/tool/terminal_tool.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

class posix_terminal_session : public terminal_session {
public:
    posix_terminal_session(const std::string& shell_path, const std::string& cwd);
    ~posix_terminal_session() override;

    bool is_alive() const override { return alive_.load(); }
    void write(const std::string& data) override;
    size_t line_count() const override;
    std::vector<std::string> read_window(size_t from_bottom, size_t to_bottom) const override;
    std::string error() const override { return err_; }

private:
    int in_fd_ = -1;
    int out_fd_ = -1;
    pid_t pid_ = -1;
    std::thread reader_;
    std::atomic<bool> alive_{false};
    std::string err_;

    mutable std::mutex mtx_;
    std::deque<std::string> lines_;
    std::string partial_;
    static constexpr size_t MAX_LINES = 5000;

    void reader_loop();
};

posix_terminal_session::posix_terminal_session(const std::string& shell_path,
                                               const std::string& cwd) {
    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    if (pipe(in_pipe) != 0) { err_ = "failed to create stdin pipe"; return; }
    if (pipe(out_pipe) != 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        err_ = "failed to create stdout pipe";
        return;
    }

    pid_ = fork();
    if (pid_ < 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        err_ = "fork failed";
        pid_ = -1;
        return;
    }
    if (pid_ == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
            std::string msg = "chdir failed: " + std::string(strerror(errno)) + "\n";
            ::write(out_pipe[1], msg.data(), msg.size());
        }
        const char* shell = shell_path.empty() ? "/bin/sh" : shell_path.c_str();
        execl(shell, shell, nullptr);
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);
    in_fd_ = in_pipe[1];
    out_fd_ = out_pipe[0];

    int flags = fcntl(out_fd_, F_GETFL, 0);
    fcntl(out_fd_, F_SETFL, flags | O_NONBLOCK);

    alive_ = true;
    reader_ = std::thread(&posix_terminal_session::reader_loop, this);
}

posix_terminal_session::~posix_terminal_session() {
    alive_ = false;
    if (pid_ > 0) {
        kill(pid_, SIGKILL);
        int status = 0;
        waitpid(pid_, &status, 0);
        pid_ = -1;
    }
    if (in_fd_ >= 0) { close(in_fd_); in_fd_ = -1; }
    if (out_fd_ >= 0) { close(out_fd_); out_fd_ = -1; }
    if (reader_.joinable()) reader_.join();
}

void posix_terminal_session::write(const std::string& data) {
    if (in_fd_ < 0) return;
    const char* p = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
        ssize_t n = ::write(in_fd_, p, remaining);
        if (n <= 0) break;
        p += n;
        remaining -= static_cast<size_t>(n);
    }
}

size_t posix_terminal_session::line_count() const {
    std::lock_guard<std::mutex> lk(mtx_);
    size_t n = lines_.size();
    if (!partial_.empty()) ++n;
    return n;
}

std::vector<std::string> posix_terminal_session::read_window(size_t from_bottom,
                                                             size_t to_bottom) const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> all(lines_.begin(), lines_.end());
    if (!partial_.empty()) all.push_back(partial_);

    size_t total = all.size();
    if (total == 0) return {};
    if (from_bottom > to_bottom) std::swap(from_bottom, to_bottom);
    if (from_bottom < 1) from_bottom = 1;
    if (to_bottom > total) to_bottom = total;
    size_t start = total - to_bottom;
    size_t end = total - from_bottom;
    std::vector<std::string> out;
    out.reserve(end - start + 1);
    for (size_t i = start; i <= end; ++i) out.push_back(all[i]);
    return out;
}

void posix_terminal_session::reader_loop() {
    char buf[4096];
    while (alive_) {
        ssize_t n = ::read(out_fd_, buf, sizeof(buf));
        if (n > 0) {
            std::string chunk(buf, static_cast<size_t>(n));
            std::lock_guard<std::mutex> lk(mtx_);
            partial_ += chunk;
            size_t pos;
            while ((pos = partial_.find('\n')) != std::string::npos) {
                std::string line = partial_.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                lines_.push_back(line);
                partial_.erase(0, pos + 1);
                if (lines_.size() > MAX_LINES) lines_.pop_front();
            }
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        } else {
            break;
        }
    }
    alive_ = false;
}

} // namespace

std::unique_ptr<terminal_session> create_terminal_session(const std::string& shell_path,
                                                          const std::string& cwd) {
    return std::make_unique<posix_terminal_session>(shell_path, cwd);
}

} // namespace agent
} // namespace spiration
