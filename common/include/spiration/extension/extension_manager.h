/**
 * @file extension_manager.h
 * @brief 拓展管理器定义。
 * @author clk
 */

#pragma once

#include <functional>
#include <ghc/filesystem.hpp>
#include <map>
#include <memory>
#include <spiration/application.h>
#include <spiration/extension/extension.hpp>
#include <spiration/init_phase.h>
#include <string>
#include <vector>

namespace spiration {

struct library_handle {
    void* mod = nullptr;
};

struct library_deleter {
    void operator()(library_handle* handle) const;
};

using library_ptr = std::unique_ptr<library_handle, library_deleter>;

class application;
class extension;
class extension_api;

/**
 * @brief 拓展管理器。
 */
class extension_manager {
public:
    /**
     * @brief 初始化拓展管理器。
     */
    extension_manager(const application& application);

    /**
     * @brief 关闭所有拓展并释放资源。
     */
    void shutdown();

    /**
     * @brief 从指定路径加载单个拓展。
     * @param path 拓展动态库路径
     * @return 是否加载并初始化成功。
     */
    bool load(const ghc::filesystem::path& path);

    /**
     * @brief 从指定目录扫描并加载所有拓展。
     * @param directory 拓展搜索目录
     * @return 成功加载的拓展数量。
     */
    size_t load_from(const ghc::filesystem::path& directory);

    /**
     * @brief 卸载指定拓展。
     * @param id 拓展 ID。
     * @return 成功则返回 `true`，否则 `false`。
     */
    bool unload_extension(const std::string& id);

    /**
     * @brief 按阶段初始化拓展。
     * @param phase 目标阶段
     * @return 成功初始化的数量
     */
    size_t initialize(init_phase phase);

    /**
     * @brief 获取所有已加载的拓展。
     */
    std::vector<extension*> extensions();

    /**
     * @brief 根据 ID 查找拓展。
     * @param id 拓展 ID。
     * @return 拓展指针，未找到返回 `nullptr`。
     */
    extension* find_extension(const std::string& id);

    /**
     * @brief 获取已加载的拓展数量。
     */
    size_t count();

    /**
     * @brief 注册内置拓展。
     * @param ext 拓展实例。
     */
    void register_builtin(std::unique_ptr<extension> ext);

    /**
     * @brief 获取指定扩展的目录路径。
     * @param id 扩展 ID。
     * @return 目录路径，未找到返回空串。
     */
    ghc::filesystem::path extension_directory(const std::string& id);

    /**
     * @brief 订阅事件。
     * @param event 事件名。
     * @param callback 回调。
     * @return 订阅 ID，用于取消订阅。
     */
    int on_event(const std::string& event,
                        std::function<void(const std::string&)> callback);

    /**
     * @brief 取消事件订阅。
     * @param subscription_id on_event 返回的 ID。
     */
    void off_event(int subscription_id);

    /**
     * @brief 发布事件。
     * @param event 事件名。
     * @param data 事件数据。
     */
    void emit_event(const std::string& event, const std::string& data);

    /**
     * @brief 注册拓展服务。
     * @param id 拓展 ID
     * @param name 服务名称
     * @param ptr 服务指针
     */
    void register_service(const std::string& id,
                                  const std::string& name, void* ptr);

    /**
     * @brief 查询指定拓展服务。
     * @param ext_id 拓展 ID
     * @param name 服务名称
     * @return 服务指针
     */
    void* get_service(const std::string& ext_id,
                             const std::string& name);

    /**
     * @brief 移除指定拓展的所有注册服务。
     * @param ext_id 拓展 ID
     */
    void unregister_services(const std::string& ext_id);

private:
    const application& application_;
    logging::logger& logger_;

    static const std::string extension_manifest_name_;
    static const std::string extension_symbol_name_;

    struct loaded_extension {
        ghc::filesystem::path path;
        library_ptr handle;
        std::unique_ptr<extension> instance;
        bool initialized = false;
    };

    std::map<std::string, loaded_extension> extensions_;
    std::map<std::string, std::map<int, std::function<void(const std::string&)>>> events_;
    int next_subscription_id_ = 1;
    std::map<std::string, std::map<std::string, void*>> services_;
    
    static void* find_symbol(library_handle* handle, const std::string& name);
    static library_ptr load_library(const ghc::filesystem::path& path);
};

} // namespace spiration
