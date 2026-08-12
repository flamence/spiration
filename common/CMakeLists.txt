file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.c"
)
file(GLOB_RECURSE COMMON_HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "include/*.hpp"
)

if(NOT (UNIX AND NOT APPLE))
    list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*opengl_renderer\\.cpp$")
endif()

add_library(spiration.common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})

set_target_properties(spiration.common PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(spiration.common PRIVATE
    CppProperties::cppproperties
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

if(WIN32)
    target_link_libraries(spiration.common PRIVATE dbghelp)
endif()

if(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    find_package(Freetype REQUIRED)
    target_include_directories(spiration.common PRIVATE
        ${OPENGL_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS} ${stb_SOURCE_DIR})
    target_link_libraries(spiration.common PRIVATE
        ${OPENGL_LIBRARIES} GLX ${FREETYPE_LIBRARIES})
endif()

target_include_directories(spiration.common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(spiration.common.headers INTERFACE)

target_include_directories(spiration.common.headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/common/include>
    $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${curl_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(spiration.common.headers INTERFACE
    nlohmann_json::nlohmann_json
)

add_library(spiration::api ALIAS spiration.common.headers)
