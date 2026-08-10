find_library(COCOA_LIBRARY Cocoa)
find_library(METAL_LIBRARY Metal)
find_library(METALKIT_LIBRARY MetalKit)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(CORE_TEXT_LIBRARY CoreText)
find_library(CORE_IMAGE_LIBRARY CoreImage)
find_library(APPKIT_LIBRARY AppKit)
find_library(UNIFORM_TYPE_ID_LIBRARY UniformTypeIdentifiers)

file(GLOB_RECURSE MACOS_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp" "src/*.c" "src/*.mm" "src/*.m" "src/*.metal"
)
file(GLOB_RECURSE MACOS_HEADERS CONFIGURE_DEPENDS
    "include/*.h" "include/*.hpp"
)

set(METAL_SRC     ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/metal_shaders.metal)
set(METAL_AIR     ${CMAKE_CURRENT_BINARY_DIR}/metal_shaders.air)
set(METALLIB_OUT  ${CMAKE_CURRENT_BINARY_DIR}/default.metallib)

add_custom_command(
    OUTPUT ${METALLIB_OUT}
    COMMAND xcrun -sdk macosx metal -c ${METAL_SRC} -o ${METAL_AIR}
    COMMAND xcrun -sdk macosx metallib ${METAL_AIR} -o ${METALLIB_OUT}
    DEPENDS ${METAL_SRC}
)
add_custom_target(compile_metallib DEPENDS ${METALLIB_OUT})

set(SPIRATION_BUNDLE_NAME       "Spiration")
set(SPIRATION_BUNDLE_IDENTIFIER "com.flamence.spiration")
set(SPIRATION_COPYRIGHT_HOLDER  "Flamence Tech.")
string(TIMESTAMP SPIRATION_COPYRIGHT_YEAR "%Y")

set(SPIRATION_LOCALIZATIONS zh-Hans)
set(_spiration_localizations_xml "")
foreach(_loc IN LISTS SPIRATION_LOCALIZATIONS)
    if(_spiration_localizations_xml)
        string(APPEND _spiration_localizations_xml "\n")
    endif()
    string(APPEND _spiration_localizations_xml "        <string>${_loc}</string>")
endforeach()

set(INFOPLIST_IN  "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist")
set(INFOPLIST_OUT "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
configure_file("${INFOPLIST_IN}" "${INFOPLIST_OUT}" @ONLY)

add_executable(spiration MACOSX_BUNDLE ${MACOS_SOURCES} ${MACOS_HEADERS})
add_dependencies(spiration compile_metallib)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(spiration PRIVATE
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

set_target_properties(spiration PROPERTIES
    OUTPUT_NAME "${SPIRATION_BUNDLE_NAME}"

    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${INFOPLIST_OUT}
    MACOSX_BUNDLE_EXECUTABLE_NAME "${SPIRATION_BUNDLE_NAME}"
    MACOSX_BUNDLE_BUNDLE_NAME "${SPIRATION_BUNDLE_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${SPIRATION_BUNDLE_IDENTIFIER}"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE_COPYRIGHT "Copyright © ${SPIRATION_COPYRIGHT_YEAR} ${SPIRATION_COPYRIGHT_HOLDER}. All rights reserved."
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${METALLIB_OUT}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/default.metallib
)
