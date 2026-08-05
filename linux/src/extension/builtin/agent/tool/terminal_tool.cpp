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

class ansi_stripper {
public:
    std::string process(const std::string& chunk) {
        std::string out;
        out.reserve(chunk.size());
        for (unsigned char c : chunk) {
            switch (state_) {
                case 0:
                    if (c == 0x1B) {
                        state_ = 1;
                    } else if (c == '\r' || c == 0x07) {
                    } else if (c < 0x20 || c == 0x7F) {
                        if (c == '\n' || c == '\t') out += static_cast<char>(c);
                    } else {
                        out += static_cast<char>(c);
                    }
                    break;
                case 1:
                    if (c == '[') state_ = 2;
                    else if (c == ']') state_ = 3;
                    else state_ = 0;
                    break;
                case 2:
                    if (c >= 0x40 && c <= 0x7E) state_ = 0;
                    break;
                case 3:
                    if (c == 0x07) state_ = 0;
                    else if (c == 0x1B) state_ = 4;
                    break;
                case 4:
                    state_ = (c == '\\') ? 0 : 3;
                    break;
            }
        }
        return out;
    }

private:
    int state_ = 0;
};

std::string sanitize_utf8(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    const size_t n = in.size();
    size_t i = 0;
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        size_t len = 0;
        if (c < 0x80) {
            out += static_cast<char>(c);
            ++i;
            continue;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (len == 0 || i + len > n) {
            out += "\xEF\xBF\xBD"; 
            ++i;
            continue;
        }
        bool ok = true;
        for (size_t k = 1; k < len; ++k) {
            unsigned char cc = static_cast<unsigned char>(in[i + k]);
            if (cc < 0x80 || cc > 0xBF) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            out += "\xEF\xBF\xBD";
            ++i;
            continue;
        }
        out.append(in, i, len);
        i += len;
    }
    return out;
}

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
        setenv("LC_ALL", "C.UTF-8", 1);
        setenv("LANG", "C.UTF-8", 1);
        setenv("PYTHONIOENCODING", "utf-8", 1);
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
    const size_t total = lines_.size() + (partial_.empty() ? 0 : 1);
    if (total == 0) return {};
    if (from_bottom > to_bottom) std::swap(from_bottom, to_bottom);
    if (from_bottom < 1) from_bottom = 1;
    if (to_bottom > total) to_bottom = total;
    const size_t start_idx = total - to_bottom;
    const size_t end_idx = total - from_bottom + 1;
    std::vector<std::string> out;
    out.reserve(end_idx - start_idx);
    for (size_t i = start_idx; i < end_idx; ++i) {
        out.push_back((i < lines_.size()) ? lines_[i] : partial_);
    }
    return out;
}

void posix_terminal_session::reader_loop() {
    char buf[4096];
    ansi_stripper strip;
    while (alive_) {
        ssize_t n = ::read(out_fd_, buf, sizeof(buf));
        if (n > 0) {
            std::string chunk(buf, static_cast<size_t>(n));
            std::string cleaned = strip.process(chunk);
            std::lock_guard<std::mutex> lk(mtx_);
            partial_ += cleaned;
            size_t pos;
            while ((pos = partial_.find('\n')) != std::string::npos) {
                std::string line = partial_.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                lines_.push_back(sanitize_utf8(line));
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
