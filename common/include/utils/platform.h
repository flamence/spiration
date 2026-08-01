/**
 * @file platform.h
 * @brief 跨平台抽象 API — 系统信息、文件系统、语言检测。
 * @author clk
 */

#pragma once

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
    static std::string app_data_dir();

    /**
     * @brief 获取可执行文件所在目录。
     */
    static std::string executable_directory();

    /**
     * @brief 拼接路径。
     * @param a 左路径
     * @param b 右路径
     * @return 拼接后的路径
     */
    static std::string join_path(const std::string& a, const std::string& b);

    /**
     * @brief 检查文件或目录是否存在。
     */
    static bool file_exists(const std::string& path);

    /**
     * @brief 创建目录。
     * @return true 创建成功或已存在
     */
    static bool create_directory(const std::string& path);

    /**
     * @brief 列出指定目录下的所有条目。
     * @param path 目录路径
     * @return 条目名称列表
     */
    static std::vector<std::string> list_directory(const std::string& path);

    /**
     * @brief 获取扩展搜索目录。
     */
    static std::string extension_directory();

    /**
     * @brief 获取系统默认语言代码。
     * @return 语言代码
     */
    static std::string system_locale();

    /**
     * @brief 使用系统默认程序打开 URL（网页、本地文件等）。
     * @param url 目标 URL 或文件路径
     */
    static void open_url(const std::string& url);

private:
    platform() = delete;
};

} // namespace spiration
