/**
 * @file terminal_tool.h
 * @brief 终端工具集。
 * @author clk
 */

#pragma once

#include <extension/builtin/agent/tool/tool.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spiration {
namespace agent {

/**
 * @brief 平台交互式终端会话。
 */
class terminal_session {
public:
    virtual ~terminal_session() = default;

    /// @brief 会话进程是否存活。
    virtual bool is_alive() const = 0;

    /// @brief 向终端写入输入。
    virtual void write(const std::string& data) = 0;

    /// @brief 当前已缓冲的输出总行数。
    virtual size_t line_count() const = 0;

    /**
     * @brief 读取输出窗口。
     * @return 按时间顺序返回的行。
     */
    virtual std::vector<std::string> read_window(size_t from_bottom,
                                                 size_t to_bottom) const = 0;

    /// @brief 最近一次错误信息。
    virtual std::string error() const = 0;
};

/**
 * @brief 终端会话快照。
 */
struct terminal_snapshot {
    std::string id;
    std::string shell;
    std::vector<std::string> lines;
};

/**
 * @brief 创建平台终端会话。
 * @param shell_path 指定的 shell 路径。
 * @param cwd 子进程初始工作目录。
 */
std::unique_ptr<terminal_session> create_terminal_session(const std::string& shell_path,
                                                          const std::string& cwd = "");

/**
 * @brief 终端管理器。
 */
class terminal_manager {
public:
    static terminal_manager& instance();

    /// @brief 创建终端会话，返回会话 id。
    std::string create(const std::string& shell, const std::string& cwd = "");
    /// @brief 向指定终端写入内容。
    std::string write(const std::string& id, const std::string& data, bool newline);
    /// @brief 读取指定终端的输出窗口。
    std::string read(const std::string& id, size_t from_bottom, size_t to_bottom);
    /// @brief 关闭指定终端会话。
    void kill(const std::string& id);
    /// @brief 导出全部会话快照。
    std::vector<terminal_snapshot> snapshots() const;
    /// @brief 关闭全部会话。
    void close_all();

private:
    terminal_manager() = default;
    mutable std::mutex mtx_;
    std::map<std::string, std::unique_ptr<terminal_session>> sessions_;
    std::map<std::string, std::string> shells_;
    size_t next_id_ = 1;
};

/// @brief 创建可持续交互的终端。
class create_terminal_tool : public tool {
public:
    std::string name() const override { return "create_terminal"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
    bool serial() const override { return true; }
};

/// @brief 向指定终端发送内容。
class write_terminal_tool : public tool {
public:
    std::string name() const override { return "write_terminal"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
    bool serial() const override { return true; }
    bool requires_approval() const override { return true; }
};

/// @brief 按窗口读取终端输出。
class read_terminal_tool : public tool {
public:
    std::string name() const override { return "read_terminal"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
    bool serial() const override { return true; }
};

/// @brief 关闭/释放指定终端。
class kill_terminal_tool : public tool {
public:
    std::string name() const override { return "kill_terminal"; }
    std::string description() const override;
    std::string parameters_json() const override;
    std::string execute(const std::string& args_json) override;
    bool serial() const override { return true; }
};

} // namespace agent
} // namespace spiration
