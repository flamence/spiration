/**
 * @file virtual_key.h
 * @brief 跨平台统一虚拟键码定义。
 * @author clk
 */

#pragma once

namespace spiration {
namespace vk {

// 与 Windows VK_* 一致
inline constexpr int Back = 0x08;     // VK_BACK
inline constexpr int Tab = 0x09;      // VK_TAB
inline constexpr int Return = 0x0D;   // VK_RETURN
inline constexpr int Escape = 0x1B;   // VK_ESCAPE
inline constexpr int Delete = 0x2E;   // VK_DELETE
inline constexpr int Home = 0x24;     // VK_HOME
inline constexpr int Left = 0x25;     // VK_LEFT
inline constexpr int Up = 0x26;       // VK_UP
inline constexpr int Right = 0x27;    // VK_RIGHT
inline constexpr int Down = 0x28;     // VK_DOWN
inline constexpr int PageUp = 0x21;   // VK_PRIOR
inline constexpr int PageDown = 0x22; // VK_NEXT
inline constexpr int End = 0x23;      // VK_END
inline constexpr int F1 = 0x70;       // VK_F1
inline constexpr int F2 = 0x71;       // VK_F2
inline constexpr int F3 = 0x72;       // VK_F3
inline constexpr int F4 = 0x73;       // VK_F4
inline constexpr int F5 = 0x74;       // VK_F5
inline constexpr int F6 = 0x75;       // VK_F6
inline constexpr int F7 = 0x76;       // VK_F7
inline constexpr int F8 = 0x77;       // VK_F8
inline constexpr int F9 = 0x78;       // VK_F9
inline constexpr int F10 = 0x79;      // VK_F10
inline constexpr int F11 = 0x7A;      // VK_F11
inline constexpr int F12 = 0x7B;      // VK_F12

} // namespace vk
} // namespace spiration
