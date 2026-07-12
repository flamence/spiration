set(STB_IMAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/stb")

find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(XCB_RANDR REQUIRED xcb-randr)

file(GLOB_RECURSE LINUX_SOURCES
    "src/*.cpp"
    "src/*.c"
)
file(GLOB_RECURSE LINUX_HEADERS
    "include/*.h"
    "include/*.hpp"
)

add_executable(${PROJECT_NAME} ${LINUX_SOURCES} ${LINUX_HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/common/src
    ${X11_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${XCB_INCLUDE_DIRS}
    ${XCB_RANDR_INCLUDE_DIRS}
    ${STB_IMAGE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    common
    ${X11_LIBRARIES}
    ${XCB_LIBRARIES}
    ${XCB_RANDR_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    GLX
    dl
    pthread
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res/spiration.png DESTINATION ${CMAKE_BINARY_DIR}/bin/res)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "spiration"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
