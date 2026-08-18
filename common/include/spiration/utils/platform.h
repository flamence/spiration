/**
 * @file platform.h
 * @brief 平台工具定义。
 * @author clk
 */

#pragma once

#include <ghc/filesystem.hpp>
#include <string>

#ifdef linux
#undef linux
#endif

namespace spiration {

/**
 * @brief 操作系统类型。
 */
enum class os_type {
    windows,
    linux,
    macos,
    ohos
};

/**
 * @brief 平台工具类。
 */
class platform {
public:
    /**
     * @return 当前操作系统类型。
     */
    static os_type current_os();

    /**
     * @return 操作系统名称。
     */
    static std::string os_name();

    /**
     * @return 操作系统版本号。
     */
    static std::string os_version();

    /**
     * @brief 获取 CPU 架构。
     */
    static std::string architecture();

    /**
     * @brief 获取应用数据目录。
     */
    static ghc::filesystem::path data_directory();

    /**
     * @brief 获取临时数据目录。
     */
    static ghc::filesystem::path temp_directory();

    /**
     * @brief 获取资源所在目录。
     */
    static ghc::filesystem::path resources_directory();

    /**
     * @brief 获取系统默认语言代码。
     * @return 语言代码
     */
    static std::string system_locale();

    /**
     * @brief 使用系统默认程序打开 URL。
     * @param url 目标 URL 或文件路径
     */
    static void open_url(const std::string& url);

private:
    platform() = delete;

#ifndef __OHOS__
    /**
     * @brief 获取可执行文件所在目录。
     */
    static ghc::filesystem::path executable_directory();
#endif
};

} // namespace spiration
