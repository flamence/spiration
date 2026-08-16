/**
 * @file level.h
 * @brief 日志等级定义。
 * @author clk
 */

#pragma once

namespace spiration {
namespace logging {

/**
 * @brief 日志等级枚举。
 */
enum level {
    trace,   ///< 追溯
    debug,   ///< 调试
    info,    ///< 信息
    warning, ///< 警告
    error    ///< 错误
};

}
}
