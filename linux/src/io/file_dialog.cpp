/**
 * @file file_dialog.cpp
 * @brief 文件对话框实现。
 * @author clk
 */

#include <io/file_dialog.h>
#include <cstdio>
#include <memory>

/**
 * @brief 转义 shell 特殊字符，防止命令注入。
 * @param s 原始字符串
 * @return 转义后的安全字符串
 */
static std::string shell_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '$':  out += "\\$"; break;
            case '`':  out += "\\`"; break;
            case '!':  out += "\\!"; break;
            default:   out += c; break;
        }
    }
    return out;
}

/**
 * @brief 执行 shell 命令并读取标准输出。
 * @param cmd 要执行的命令
 * @return 命令的标准输出，已去除尾部换行符；失败时返回空串
 */
static std::string popen_read(const char* cmd) {
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return {};

    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe.get())) {
        result += buf;
    }
    while (!result.empty() &&
           (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

namespace spiration {
namespace io {

std::string open_file(const std::string& title,
                      const std::string& filter,
                      const std::vector<std::string>& patterns) {
    std::string cmd = "zenity --file-selection --title=\"";
    cmd += shell_escape(title);
    cmd += "\"";

    if (!patterns.empty() && patterns[0] != "*") {
        cmd += " --file-filter=\"";
        cmd += shell_escape(filter);
        cmd += " | ";
        for (size_t i = 0; i < patterns.size(); ++i) {
            if (i > 0) cmd += " ";
            cmd += shell_escape(patterns[i]);
        }
        cmd += "\"";
    }

    return popen_read(cmd.c_str());
}

std::string save_file(const std::string& title,
                      const std::string& filter,
                      const std::vector<std::string>& patterns) {
    std::string cmd = "zenity --file-selection --save --title=\"";
    cmd += shell_escape(title);
    cmd += "\"";

    if (!patterns.empty() && patterns[0] != "*") {
        cmd += " --file-filter=\"";
        cmd += shell_escape(filter);
        cmd += " | ";
        for (size_t i = 0; i < patterns.size(); ++i) {
            if (i > 0) cmd += " ";
            cmd += shell_escape(patterns[i]);
        }
        cmd += "\"";
    }

    return popen_read(cmd.c_str());
}

} // namespace io
} // namespace spiration