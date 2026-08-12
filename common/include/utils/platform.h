/**
 * @file platform.h
 * @brief 平台工具定义。
 * @author clk
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#ifdef linux
#undef linux // 解决 OpenHarmony NDK 冲突
#endif

namespace spiration {

/**
 * @brief 操作系统类型。
 */
enum class os_type {
    windows,
    linux,
    macos,
    ohos,
    unknown
};

/**
 * @brief 跨平台工具类。
 *
 * 提供平台无关的系统信息查询、文件系统操作、语言检测等功能。
 * 所有方法均为静态。
 */
class platform {
public:
    /**
     * @brief 获取当前操作系统类型。
     */
    static os_type current_os();

    /**
     * @brief 获取操作系统名称。
     */
    static std::string os_name();

    /**
     * @brief 获取操作系统版本号。
     */
    static std::string os_version();

    /**
     * @brief 获取 CPU 架构。
     */
    static std::string architecture();

    /**
     * @brief 获取应用数据目录。
     */
    static std::filesystem::path data_directory();

    /**
     * @brief 获取临时数据目录。
     */
    static std::filesystem::path temp_directory();

    /**
     * @brief 获取资源所在目录。
     */
    static std::filesystem::path resource_directory();

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
};

} // namespace spiration
