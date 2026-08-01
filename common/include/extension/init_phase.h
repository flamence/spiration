/**
 * @file init_phase.h
 * @brief 拓展初始化阶段枚举。
 * @author clk
 */

#pragma once

namespace spiration {

/**
 * @brief 拓展初始化阶段。
 */
enum class init_phase {
    early, ///< 此阶段仅加载了拓展。
    normal ///< 此阶段窗口、控件已加载完毕。
};

}
