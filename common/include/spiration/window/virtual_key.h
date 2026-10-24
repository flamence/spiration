/**
 * @file virtual_key.h
 * @brief 虚拟键码定义。
 * @author clk
 */

#pragma once

namespace spiration {
namespace vk {

// 与 Windows VK_* 一致
static constexpr int Back = 0x08;     // VK_BACK
static constexpr int Tab = 0x09;      // VK_TAB
static constexpr int Return = 0x0D;   // VK_RETURN
static constexpr int Escape = 0x1B;   // VK_ESCAPE
static constexpr int Delete = 0x2E;   // VK_DELETE
static constexpr int Home = 0x24;     // VK_HOME
static constexpr int Left = 0x25;     // VK_LEFT
static constexpr int Up = 0x26;       // VK_UP
static constexpr int Right = 0x27;    // VK_RIGHT
static constexpr int Down = 0x28;     // VK_DOWN
static constexpr int PageUp = 0x21;   // VK_PRIOR
static constexpr int PageDown = 0x22; // VK_NEXT
static constexpr int End = 0x23;      // VK_END
static constexpr int F1 = 0x70;       // VK_F1
static constexpr int F2 = 0x71;       // VK_F2
static constexpr int F3 = 0x72;       // VK_F3
static constexpr int F4 = 0x73;       // VK_F4
static constexpr int F5 = 0x74;       // VK_F5
static constexpr int F6 = 0x75;       // VK_F6
static constexpr int F7 = 0x76;       // VK_F7
static constexpr int F8 = 0x77;       // VK_F8
static constexpr int F9 = 0x78;       // VK_F9
static constexpr int F10 = 0x79;      // VK_F10
static constexpr int F11 = 0x7A;      // VK_F11
static constexpr int F12 = 0x7B;      // VK_F12

// 字母与数字：值等于 ASCII 大写码，便于快捷键配置直接写出。
static constexpr int A = 0x41;
static constexpr int T = 0x54;
static constexpr int W = 0x57;
static constexpr int Digit1 = 0x31;
static constexpr int Digit9 = 0x39;

} // namespace vk
} // namespace spiration
