/**
 * @file extension_manager.h
 * @brief 扩展管理器，负责加载、卸载、枚举扩展。
 * @author clk
 */

#pragma once

#include <extension/extension_loader.h>
#include <string>
#include <vector>
#include <memory>

namespace spiration {

class extension;
class extension_api;

/**
 * @brief 扩展管理器。
 */
class extension_manager {
public:
    /**
     * @brief 初始化扩展管理器。
     * @param api 扩展 API 上下文，传递给所有扩展
     */
    static void initialize(std::shared_ptr<extension_api> api);

    /**
     * @brief 关闭所有扩展并释放资源。
     */
    static void shutdown();

    /**
     * @brief 从指定路径加载单个扩展。
     * @param path 扩展动态库路径
     * @return true 加载并初始化成功
     */
    static bool load_extension(const std::string& path);

    /**
     * @brief 从指定目录扫描并加载所有扩展。
     * @param directory 扩展搜索目录
     * @return 成功加载的扩展数量
     */
    static size_t load_extensions_from(const std::string& directory);

    /**
     * @brief 卸载指定扩展。
     * @param id 扩展 ID
     * @return true 卸载成功
     */
    static bool unload_extension(const std::string& id);

    /**
     * @brief 初始化所有已加载但未初始化的扩展。
     * @return 成功初始化的数量
     */
    static size_t initialize_all();

    /**
     * @brief 关闭所有扩展。
     */
    static void shutdown_all();

    /**
     * @brief 获取所有已加载的扩展。
     */
    static std::vector<extension*> extensions();

    /**
     * @brief 根据 ID 查找扩展。
     * @param id 扩展 ID
     * @return 扩展指针，未找到返回 nullptr
     */
    static extension* find_extension(const std::string& id);

    /**
     * @brief 获取已加载的扩展数量。
     */
    static size_t count();

private:
    struct loaded_extension {
        extension_loader::lib_handle handle;
        extension* instance = nullptr;
        bool initialized = false;
    };

    static std::vector<loaded_extension> s_extensions;
    static std::shared_ptr<extension_api> s_api;
    static bool s_initialized;
};

} // namespace spiration
