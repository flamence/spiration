/**
 * @file init_phase.h
 * @brief 初始化阶段定义。
 * @author clk
 */

#pragma once

namespace spiration {

/**
 * @brief 初始化阶段。
 */
enum class init_phase {
    early, ///< 早期阶段，仅加载了拓展。
    normal, ///< 中期阶段，窗口、控件在此阶段加载，且拓展在窗口显示前被加载。
    late ///< 晚期阶段，窗口已经显示出来。
};

}
