cmake_minimum_required(VERSION 3.5.0)
project(spiration)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(SPIRATION_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

file(GLOB_RECURSE OHOS_HEADERS
    "${NATIVERENDER_ROOT_PATH}/include/*.h"
    "${NATIVERENDER_ROOT_PATH}/include/*.hpp"
)

file(GLOB_RECURSE OHOS_SOURCES
    "${NATIVERENDER_ROOT_PATH}/src/*.c"
    "${NATIVERENDER_ROOT_PATH}/src/*.cpp"
)

# ---- 共享核心源码（common）----
# OHOS 环境没有 libcurl / CppProperties，并且以下文件由 OHOS 侧
# 自有实现替代（ohos_chat.cpp / web_tool.cpp / terminal_tool.cpp / platform.cpp），
# 因此需要从 common 中排除，避免重复符号或引入桌面依赖：
#   chat.cpp        —— 使用 libcurl，由 ohos_chat.cpp 提供 OHOS 占位实现
#   tool/web_tool.cpp       —— 使用 libcurl，由 OHOS 版本替代
#   tool/terminal_tool.cpp  —— 由 OHOS 版本替代
#   utils/platform.cpp      —— 由 OHOS 版本替代
file(GLOB_RECURSE SPIRATION_COMMON_SOURCES
    "${SPIRATION_ROOT_PATH}/common/src/*.cpp"
    "${SPIRATION_ROOT_PATH}/common/src/*.c"
)
list(FILTER SPIRATION_COMMON_SOURCES EXCLUDE REGEX ".*/chat\\.cpp$")
list(FILTER SPIRATION_COMMON_SOURCES EXCLUDE REGEX ".*/web_tool\\.cpp$")
list(FILTER SPIRATION_COMMON_SOURCES EXCLUDE REGEX ".*/terminal_tool\\.cpp$")
list(FILTER SPIRATION_COMMON_SOURCES EXCLUDE REGEX ".*/platform\\.cpp$")

add_library(entry SHARED ${OHOS_HEADERS} ${OHOS_SOURCES} ${SPIRATION_COMMON_SOURCES})

target_include_directories(entry PUBLIC
    ${NATIVERENDER_ROOT_PATH}/include
    ${SPIRATION_ROOT_PATH}/common/include
    ${NATIVERENDER_ROOT_PATH}/deps/stb
    ${NATIVERENDER_ROOT_PATH}/deps/json/include
)

add_custom_command(TARGET entry PRE_BUILD COMMAND
    ${CMAKE_COMMAND} -E copy_directory
    ${SPIRATION_ROOT_PATH}/common/lang/
    ${NATIVERENDER_ROOT_PATH}/../resources/rawfile/lang/
)

target_link_libraries(entry PUBLIC
                                   libace_napi.z.so
                                   libEGL.so
                                   libGLESv3.so
                                   libnative_window.so
                                   libnative_drawing.so
                                   librawfile.z.so
                                   libhilog_ndk.z.so)
