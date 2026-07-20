/**
 * @file extension_loader.h
 * @brief 拓展加载器。
 * @author clk
 */

#pragma once

#include <string>
#include <memory>

namespace spiration {

class extension;
class extension_api;
struct manifest_data;

/**
 * @brief 拓展加载器。
 */
class extension_loader {
public:
    /**
     * @brief 动态库句柄。
     */
    struct library_handle {
        void* mod = nullptr;
    };

    /**
     * @brief 动态库句柄删除器。
     */
    struct library_deleter {
        void operator()(library_handle* handle) const;
    };

    using lib_handle = std::unique_ptr<library_handle, library_deleter>;

    /**
     * @brief 加载动态库。
     * @param path 动态库路径
     * @return 库句柄，失败返回 nullptr
     */
    static lib_handle load_library(const std::string& path);

    /**
     * @brief 从已加载的动态库中查找符号地址。
     * @param handle 库句柄
     * @param symbol_name 符号名称
     * @return 符号地址，未找到返回 nullptr
     */
    static void* find_symbol(library_handle* handle, const std::string& symbol_name);

    /**
     * @brief 扩展加载结果，包含动态库句柄和扩展实例。
     */
    struct load_result {
        lib_handle handle = lib_handle(nullptr);
        extension* instance = nullptr;
    };

    /**
     * @brief 从动态库加载扩展实例。
     * @param path 动态库路径
     * @return 加载结果，失败时 instance 为 nullptr
     */
    static load_result load_extension_from(const std::string& path);

    /**
     * @brief 从扩展目录加载。
     * @param dir_path 扩展目录路径
     * @param out_manifest 解析后的清单
     * @return 加载结果，失败时 instance 为 nullptr
     */
    static load_result load_extension_from_dir(const std::string& dir_path,
                                               manifest_data* out_manifest = nullptr);

    /**
     * @brief 读取文本文件内容。
     * @param path 文件路径
     * @return 文件内容，失败返回空字符串
     */
    static std::string read_file_text(const std::string& path);

    /**
     * @brief 获取错误信息。
     */
    static std::string last_error();

private:
    static std::string s_last_error;
};

} // namespace spiration
