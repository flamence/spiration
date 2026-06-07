/**
 * @file extension.h
 * @brief 扩展系统接口（预留）。
 * @author clk
 */

#pragma once

#include <string>

namespace spiration {

/**
 * @brief 扩展基类（预留）。
 *
 * 为未来插件化扩展体系预留的接口框架。
 */
class extension {
public:
    std::string id;
};

}