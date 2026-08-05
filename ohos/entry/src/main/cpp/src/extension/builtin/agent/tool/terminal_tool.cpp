/**
 * @file terminal_tool.cpp
 * @brief 终端会话实现。
 * @author clk
 */

#include <extension/builtin/agent/tool/terminal_tool.h>

namespace spiration {
namespace agent {

std::unique_ptr<terminal_session> create_terminal_session(const std::string&,
                                                          const std::string&) {
    return nullptr;
}

} // namespace agent
} // namespace spiration
