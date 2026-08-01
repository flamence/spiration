/**
 * @file extension.h
 * @brief 拓展基类。
 * @author clk
 */

#pragma once

#include <extension/extension_api.h>
#include <extension/init_phase.h>

#include <memory>
#include <string>

namespace spiration {

/**
 * @brief 拓展基类。
 */
class extension {
public:
    virtual ~extension() = default;

    /**
     * @brief 唯一标识符。
     */
    virtual std::string id() const = 0;

    /**
     * @brief 显示名称。
     */
    virtual std::string name() const = 0;

    /**
     * @brief 版本号。
     */
    virtual std::string version() const = 0;

    /**
     * @brief 描述。
     */
    virtual std::string description() const = 0;

    /**
     * @brief 声明应在何时被初始化。
     */
    virtual init_phase phase() const { return init_phase::normal; }

    /**
     * @brief 设置拓展 API 上下文。
     * @param new_api 拓展 API 上下文。
     */
    void set_api(std::unique_ptr<extension_api> new_api) { api = std::move(new_api); }

    /**
     * @brief 初始化。
     * @return 成功返回 `true` ，否则 `false`。
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭。
     */
    virtual void shutdown() = 0;

    /**
     * @brief 注册一个命名服务，供其他拓展查询。
     * @tparam T 服务类型
     * @param name 服务名称
     * @param ptr  服务指针
     */
    template<typename T>
    void register_service(const std::string& name, T* ptr) {
        register_service_impl(name, static_cast<void*>(ptr));
    }

    /**
     * @brief 查询其他拓展暴露的服务。
     * @tparam T 期望的服务类型
     * @param name 服务名称
     * @return 服务指针，不存在或类型不匹配返回 nullptr
     */
    template<typename T>
    T* get_service(const std::string& name) const {
        return static_cast<T*>(get_service_impl(name));
    }

protected:
    std::unique_ptr<extension_api> api = nullptr;

private:
    /** @brief 注册服务的类型擦除实现。 */
    void register_service_impl(const std::string& name, void* ptr);
    /** @brief 查询服务的类型擦除实现。 */
    void* get_service_impl(const std::string& name) const;
};

}