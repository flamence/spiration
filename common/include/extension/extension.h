/**
 * @file extension.h
 * @brief 扩展系统抽象基类。定义扩展生命周期与元数据接口。
 * @author clk
 */

#pragma once

#include <string>

namespace spiration {

class extension_api;

/**
 * @brief 拓展基类。
 */
class extension {
public:
    virtual ~extension() = default;

    /**
     * @brief 拓展唯一标识符。
     */
    virtual std::string id() const = 0;

    /**
     * @brief 拓展名称。
     */
    virtual std::string name() const = 0;

    /**
     * @brief 拓展版本号。
     */
    virtual std::string version() const = 0;

    /**
     * @brief 拓展描述。
     */
    virtual std::string description() const = 0;

    /**
     * @brief 设置拓展 API 上下文。在 initialize() 之前调用。
     */
    void set_api(extension_api* api) { api_ = api; }

    /**
     * @brief 获取拓展 API 上下文。
     */
    extension_api* get_api() const { return api_; }

    /**
     * @brief 初始化拓展。此时可通过 get_api() 访问宿主功能。
     * @return true 初始化成功，false 失败
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭拓展。在卸载前调用，用于释放资源。
     */
    virtual void shutdown() = 0;

protected:
    extension_api* api_ = nullptr;
};

/**
 * @brief 拓展创建函数类型。
 */
using extension_create_func = extension* (*)();

/**
 * @brief 拓展销毁函数类型。
 */
using extension_destroy_func = void (*)(extension*);

}