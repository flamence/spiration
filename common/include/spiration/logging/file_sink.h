/**
 * @file file_sink.h
 * @brief 文件接收器定义。
 * @author clk
 */

#pragma once

#include <fstream>
#include <ghc/filesystem.hpp>
#include <string>
#include <spiration/logging/sink.h>

namespace spiration {
namespace logging {

/**
 * @brief 文件接收器。
 *
 * @note 打开失败**不会**抛异常（`std::ofstream` 的语义），因此本类显式记录
 *       失败原因并写往 stderr：否则一旦路径不可写（权限、磁盘满、目录被删），
 *       日志会无声消失，只剩控制台有输出，很难定位。
 */
class file_sink : public sink {
public:
    explicit file_sink(const ghc::filesystem::path& path);

    ~file_sink() override;

    void write(const log& log, log_formatter formatter) override;

    /**
     * @brief 目标文件当前是否可写。
     */
    bool is_open() const { return file_.is_open(); }

    /**
     * @return 日志文件路径。
     */
    const ghc::filesystem::path& path() const { return path_; }

private:
    /**
     * @brief 把失败原因写往 stderr。
     * @note 只在首次失败时输出一次，避免重复刷屏。
     */
    void report(const char* reason);

    ghc::filesystem::path path_;
    std::ofstream file_;
    /** 已就失败原因报过一次，避免每次写入都刷屏。 */
    bool reported_ = false;
};

}
}
