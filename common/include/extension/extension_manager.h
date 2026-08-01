/**
 * @file extension_manager.h
 * @brief 拓展管理器。
 * @author clk
 */

#pragma once

#include <extension/extension_loader.h>
#include <extension/init_phase.h>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace spiration {

class extension;
class extension_api;

/**
 * @brief 拓展管理器。
 */
class extension_manager {
public:
    /**
     * @brief 获取实例。
     */
    static extension_manager& instance();

    /**
     * @brief 关闭所有拓展并释放资源。
     */
    static void shutdown();

    /**
     * @brief 从指定路径加载单个拓展。
     * @param path 拓展动态库路径
     * @return true 加载并初始化成功
     */
    static bool load_extension(const std::string& path);

    /**
     * @brief 从指定目录扫描并加载所有拓展。
     * @param directory 拓展搜索目录。
     * @return 成功加载的拓展数量。
     */
    static size_t load_extensions_from(const std::string& directory);

    /**
     * @brief 卸载指定拓展。
     * @param id 拓展 ID。
     * @return 成功则返回 `true`，否则 `false`。
     */
    static bool unload_extension(const std::string& id);

    /**
     * @brief 初始化所有已加载但未初始化的拓展。
     * @return 成功初始化的数量
     */
    static size_t initialize_all();

    /**
     * @brief 按阶段初始化拓展。
     * @param phase 目标阶段
     * @return 成功初始化的数量
     */
    static size_t initialize_phase(init_phase phase);

    /**
     * @brief 关闭所有拓展。
     */
    static void shutdown_all();

    /**
     * @brief 获取所有已加载的拓展。
     */
    static std::vector<extension*> extensions();

    /**
     * @brief 根据 ID 查找拓展。
     * @param id 拓展 ID。
     * @return 拓展指针，未找到返回 `nullptr`。
     */
    static extension* find_extension(const std::string& id);

    /**
     * @brief 获取已加载的拓展数量。
     */
    static size_t count();

    /**
     * @brief 注册内置拓展。
     * @param ext 拓展实例。
     */
    static void register_builtin(std::unique_ptr<extension> ext);

    /**
     * @brief 获取指定扩展的目录路径。
     * @param id 扩展 ID。
     * @return 目录路径，未找到返回空串。
     */
    static std::string extension_directory(const std::string& id);

    /**
     * @brief 订阅事件。
     * @param event 事件名。
     * @param callback 回调。
     * @return 订阅 ID，用于取消订阅。
     */
    static int on_event(const std::string& event,
                        std::function<void(const std::string&)> callback);

    /**
     * @brief 取消事件订阅。
     * @param subscription_id on_event 返回的 ID。
     */
    static void off_event(int subscription_id);

    /**
     * @brief 发布事件。
     * @param event 事件名。
     * @param data 事件数据。
     */
    static void emit_event(const std::string& event, const std::string& data);

    /**
     * @brief 注册拓展服务。
     * @param ext_id 拓展 ID
     * @param name   服务名称。
     * @param ptr    服务指针。
     */
    static void register_service(const std::string& ext_id,
                                  const std::string& name, void* ptr);

    /**
     * @brief 查询指定拓展服务。
     * @param ext_id 拓展 ID。
     * @param name   服务名称。
     * @return 服务指针，未找到返回 `nullptr`。
     */
    static void* get_service(const std::string& ext_id,
                             const std::string& name);

    /**
     * @brief 移除指定拓展的所有注册服务。
     * @param ext_id 拓展 ID
     */
    static void unregister_services(const std::string& ext_id);

private:
    /**
     * @brief 初始化拓展管理器（注册内置扩展）。私有：只能通过 instance() 创建。
     */
    extension_manager();

    /** @brief 已加载的扩展信息。 */
    struct loaded_extension {
        /** @brief 动态库句柄。 */
        extension_loader::lib_handle handle;
        /** @brief 扩展实例指针。 */
        extension* instance = nullptr;
        /** @brief 是否已调用 initialize()。 */
        bool initialized = false;
        /** @brief 扩展所在目录路径。 */
        std::string dir_path;
    };

    /** @brief 所有已加载的扩展列表。 */
    static std::vector<loaded_extension> extensions_;
    /** @brief 管理器是否已初始化。 */
    static bool initialized_;

    /** @brief 事件订阅表。 */
    static std::map<std::string, std::map<int, std::function<void(const std::string&)>>> events_;
    /** @brief 下一个可用的订阅 ID。 */
    static int next_subscription_id_;
    /** @brief 拓展服务注册表: ext_id -> (name -> ptr)。 */
    static std::map<std::string, std::map<std::string, void*>> services_;
};

} // namespace spiration
