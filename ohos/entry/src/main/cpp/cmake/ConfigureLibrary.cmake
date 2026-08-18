file(GLOB_RECURSE OHOS_HEADERS
    "${NATIVERENDER_ROOT_PATH}/include/*.h"
    "${NATIVERENDER_ROOT_PATH}/include/*.hpp"
)

file(GLOB_RECURSE OHOS_SOURCES
    "${NATIVERENDER_ROOT_PATH}/src/*.c"
    "${NATIVERENDER_ROOT_PATH}/src/*.cpp"
)

add_library(entry SHARED ${OHOS_HEADERS} ${OHOS_SOURCES})

# entry 自有头文件（napi_bridge.h / spiration/net/ohos_http.h 等）与本地依赖
target_include_directories(entry PRIVATE
    ${NATIVERENDER_ROOT_PATH}/include
    ${NATIVERENDER_ROOT_PATH}/deps/stb
    ${NATIVERENDER_ROOT_PATH}/deps/json/include
)

add_custom_command(TARGET entry PRE_BUILD COMMAND
    ${CMAKE_COMMAND} -E copy_directory
    ${SPIRATION_ROOT_PATH}/common/lang/
    ${NATIVERENDER_ROOT_PATH}/../resources/rawfile/lang/
)

target_link_libraries(entry PRIVATE
        libace_napi.z.so
        libEGL.so
        libGLESv3.so
        libnative_window.so
        libnative_drawing.so
        librawfile.z.so
        libhilog_ndk.z.so)

target_link_libraries(entry PUBLIC
        spiration::core)
