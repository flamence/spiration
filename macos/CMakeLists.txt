find_library(COCOA_LIBRARY Cocoa)
find_library(METAL_LIBRARY Metal)
find_library(METALKIT_LIBRARY MetalKit)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(CORE_TEXT_LIBRARY CoreText)
find_library(CORE_IMAGE_LIBRARY CoreImage)
find_library(APPKIT_LIBRARY AppKit)
find_library(OPENGL_LIBRARY OpenGL)
# OpenGL 渲染器的 FreeType 文本管线（opengl_text_freetype.cpp）编译于本目标。
find_package(Freetype REQUIRED)

file(GLOB_RECURSE MACOS_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp" "src/*.c" "src/*.mm" "src/*.m" "src/*.metal"
)
file(GLOB_RECURSE MACOS_HEADERS CONFIGURE_DEPENDS
    "include/*.h" "include/*.hpp"
)

set(METAL_SRC     ${CMAKE_CURRENT_SOURCE_DIR}/src/spiration/renderer/metal_shaders.metal)
set(METAL_AIR     ${CMAKE_CURRENT_BINARY_DIR}/metal_shaders.air)
set(METALLIB_OUT  ${CMAKE_CURRENT_BINARY_DIR}/default.metallib)

add_custom_command(
    OUTPUT ${METALLIB_OUT}
    COMMAND xcrun -sdk macosx metal -c -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} ${METAL_SRC} -o ${METAL_AIR}
    COMMAND xcrun -sdk macosx metallib ${METAL_AIR} -o ${METALLIB_OUT}
    DEPENDS ${METAL_SRC}
)
add_custom_target(compile_metallib DEPENDS ${METALLIB_OUT})

set(SPIRATION_BUNDLE_NAME       "${SPIRATION_APP_NAME}")
set(SPIRATION_BUNDLE_IDENTIFIER "${SPIRATION_BUNDLE_IDENTIFIER}")
set(SPIRATION_COPYRIGHT_HOLDER  "${SPIRATION_VENDOR}")
string(TIMESTAMP SPIRATION_COPYRIGHT_YEAR "%Y")
set(_spiration_localizations_xml "")
foreach(_loc IN LISTS SPIRATION_LOCALIZATIONS)
    if(_spiration_localizations_xml)
        string(APPEND _spiration_localizations_xml "\n")
    endif()
    string(APPEND _spiration_localizations_xml "        <string>${_loc}</string>")
endforeach()

set(_spiration_localizations_xml "")
foreach(_loc IN LISTS SPIRATION_LOCALIZATIONS)
    if(_spiration_localizations_xml)
        string(APPEND _spiration_localizations_xml "\n")
    endif()
    string(APPEND _spiration_localizations_xml "        <string>${_loc}</string>")
endforeach()
set(SPIRATION_LOCALIZATIONS_XML "${_spiration_localizations_xml}")

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
    ${FREETYPE_INCLUDE_DIRS}
)
target_compile_definitions(${PROJECT_NAME} PRIVATE SPIRATION)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spiration::core
    ${COCOA_LIBRARY}
    ${METAL_LIBRARY}
    ${METALKIT_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${CORE_TEXT_LIBRARY}
    ${CORE_IMAGE_LIBRARY}
    ${APPKIT_LIBRARY}
    ${OPENGL_LIBRARY}
    ${FREETYPE_LIBRARIES}
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
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/lang
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../Resources/lang"
    COMMAND codesign --force --deep --sign - "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../.."
    COMMAND xattr -cr "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../.."
)
