file(GLOB_RECURSE COMMON_SOURCES 
    "src/*.cpp" 
    "src/*.c"
)
file(GLOB_RECURSE COMMON_HEADERS 
    "include/*.h" 
    "include/*.hpp"
)

add_library(spiration-common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})

set_target_properties(spiration-common PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(spiration-common PUBLIC CppProperties::cppproperties)
target_link_libraries(spiration-common PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(spiration-common PUBLIC CURL::libcurl)

if(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    find_package(Freetype REQUIRED)
    target_include_directories(spiration-common PRIVATE
        ${OPENGL_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS} ${stb_SOURCE_DIR})
    target_link_libraries(spiration-common PRIVATE
        ${OPENGL_LIBRARIES} GLX ${FREETYPE_LIBRARIES})
endif()

target_include_directories(spiration-common
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(spiration-common PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${COMMON_HEADERS}"
)