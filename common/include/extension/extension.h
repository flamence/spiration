/**
 * @file extension.h
 * @brief 扩展系统抽象基类。定义扩展生命周期与元数据接口。
 * @author clk
 */

#pragma once

#include <string>

namespace spiration {

/**
 * @brief 扩展基类。
 *
 * 所有扩展必须继承此类并实现全部纯虚方法。
 */
class extension_api;

class extension {
public:
    virtual ~extension() = default;

    /**
     * @brief 扩展唯一标识符。
     */
    virtual std::string id() const = 0;

    /**
     * @brief 扩展名称。
     */
    virtual std::string name() const = 0;

    /**
     * @brief 扩展版本号。
     */
    virtual std::string version() const = 0;

    /**
     * @brief 扩展描述。
     */
    virtual std::string description() const = 0;

    /**
     * @brief 设置扩展 API 上下文。在 initialize() 之前调用。
     */
    void set_api(extension_api* api) { api_ = api; }

    /**
     * @brief 获取扩展 API 上下文。
     */
    extension_api* get_api() const { return api_; }

    /**
     * @brief 初始化扩展。此时可通过 get_api() 访问宿主功能。
     * @return true 初始化成功，false 失败
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭扩展。在卸载前调用，用于释放资源。
     */
    virtual void shutdown() = 0;

protected:
    extension_api* api_ = nullptr;
};

/**
 * @brief 扩展创建函数类型。
 */
using extension_create_func = extension* (*)();

/**
 * @brief 扩展销毁函数类型。
 */
using extension_destroy_func = void (*)(extension*);

}