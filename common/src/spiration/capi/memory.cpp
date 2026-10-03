/**
 * @file memory.cpp
 * @brief 宿主函数表 · 内存（`alloc` / `realloc` / `dealloc`）。
 * @author clk
 *
 * 拓展经此分配宿主堆内存，从而避免「在你的 CRT 里分配、在我的 CRT 里释放」。
 */

#include "internal.h"

#include <cstdlib>
namespace spiration {
namespace capi {

void* SPIRATION_CALL host_alloc(const spiration_extension_desc* self,
                                size_t size) {
    SPIRATION_UNUSED(self);
    return std::malloc(size == 0u ? 1u : size);
}

void* SPIRATION_CALL host_realloc(const spiration_extension_desc* self,
                                  void* block, size_t size) {
    SPIRATION_UNUSED(self);
    return std::realloc(block, size == 0u ? 1u : size);
}

void SPIRATION_CALL host_dealloc(const spiration_extension_desc* self,
                                 void* block) {
    SPIRATION_UNUSED(self);
    std::free(block);
}

} // namespace capi
} // namespace spiration
