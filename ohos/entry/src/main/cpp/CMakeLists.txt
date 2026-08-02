cmake_minimum_required(VERSION 3.5.0)
project(spiration)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(SPIRATION_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

file(GLOB_RECURSE COMMON_SOURCES
    "${SPIRATION_ROOT_PATH}/common/src/*.c"
    "${SPIRATION_ROOT_PATH}/common/src/*.cpp"
)

# 排除依赖 curl 的文件和桌面平台 application
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*chat\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*web_tool\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/common/src/application\\.cpp$")
# 排除 Windows 版本的 clipboard 实现
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/common/src/utils/clipboard\\.cpp$")
file(GLOB_RECURSE OHOS_SOURCES
    "${NATIVERENDER_ROOT_PATH}/src/*.c"
    "${NATIVERENDER_ROOT_PATH}/src/*.cpp"
)

add_library(entry SHARED ${COMMON_SOURCES} ${OHOS_SOURCES})

# 定义 OHOS 平台宏，禁用 curl 依赖
target_compile_definitions(entry PRIVATE OHOS_PLATFORM)

target_include_directories(entry PRIVATE
    ${NATIVERENDER_ROOT_PATH}/include
    ${SPIRATION_ROOT_PATH}/common/include
    ${SPIRATION_ROOT_PATH}/linux/deps/stb
    ${SPIRATION_ROOT_PATH}/build/_deps/json-src/include
)

add_custom_command(TARGET entry PRE_BUILD COMMAND
    ${CMAKE_COMMAND} -E copy_directory
    ${SPIRATION_ROOT_PATH}/common/lang/
    ${NATIVERENDER_ROOT_PATH}/../resources/rawfile/lang/
)

target_link_libraries(entry PUBLIC libace_napi.z.so
                                   libEGL.so
                                   libGLESv3.so
                                   libnative_window.so
                                   libnative_drawing.so
                                   librawfile.z.so
                                   libhilog_ndk.z.so)