/**
 * @file opengl_platform.h
 * @brief macOS OpenGL 平台桥接声明（实现见 macos/src/spiration/renderer/opengl_platform.mm）。
 * @author clk
 *
 * 仅供 macOS 上的 OpenGL 渲染器使用；其余平台不引用此头文件。
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建绑定到指定 NSView 的 NSOpenGLContext（4.1 Core Profile，双缓冲）。
 * @param ns_view NSView 指针。
 * @return 上下文句柄（失败返回 NULL）。调用方负责通过 spiration_gl_destroy 释放。
 */
void* spiration_gl_create_context(void* ns_view);

/**
 * @brief 将指定上下文设为当前线程的 GL 上下文。
 * @param context 上下文句柄。
 * @return 非 0 表示成功。
 */
int spiration_gl_make_current(void* context);

/**
 * @brief 交换前后缓冲（flushBuffer），应在每帧绘制结束后调用。
 */
void spiration_gl_flush(void* context);

/**
 * @brief 视图尺寸/backing 变化后调用（对应 [NSOpenGLContext update]）。
 */
void spiration_gl_update(void* context);

/**
 * @brief 销毁上下文并解除与视图的绑定。
 */
void spiration_gl_destroy(void* context);

#ifdef __cplusplus
} // extern "C"
#endif
