find_library(COCOA_LIBRARY Cocoa)
find_library(METAL_LIBRARY Metal)
find_library(METALKIT_LIBRARY MetalKit)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(CORE_TEXT_LIBRARY CoreText)
find_library(CORE_IMAGE_LIBRARY CoreImage)
find_library(APPKIT_LIBRARY AppKit)
find_library(UNIFORM_TYPE_ID_LIBRARY UniformTypeIdentifiers)

file(GLOB_RECURSE MACOS_SOURCES
    "src/*.cpp"
    "src/*.c"
    "src/*.mm"
    "src/*.m"
    "src/*.metal"
)
file(GLOB_RECURSE MACOS_HEADERS
    "include/*.h"
    "include/*.hpp"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/default.metallib
    COMMAND xcrun -sdk macosx metal
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/metal_shaders.metal
        -o ${CMAKE_CURRENT_BINARY_DIR}/metal_shaders.air
    COMMAND xcrun -sdk macosx metallib
        ${CMAKE_CURRENT_BINARY_DIR}/metal_shaders.air
        -o ${CMAKE_CURRENT_BINARY_DIR}/default.metallib
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/metal_shaders.metal
)

add_custom_target(compile_metallib DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/default.metallib)

add_executable(${PROJECT_NAME} ${MACOS_SOURCES} ${MACOS_HEADERS})

add_dependencies(${PROJECT_NAME} compile_metallib)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spiration-common
    ${COCOA_LIBRARY}
    ${METAL_LIBRARY}
    ${METALKIT_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${CORE_TEXT_LIBRARY}
    ${CORE_IMAGE_LIBRARY}
    ${APPKIT_LIBRARY}
    ${UNIFORM_TYPE_ID_LIBRARY}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/default.metallib
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/default.metallib
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/lang
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/lang
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "Spiration"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../bin
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.flamence.spiration"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist
)

spiration_setup_runtime(${PROJECT_NAME})
