cmake_minimum_required(VERSION 3.16)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
endif()
project(spiration VERSION 0.63 LANGUAGES C CXX)

include(FetchContent)

cmake_policy(SET CMP0135 NEW)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
  CppProperties
  GIT_REPOSITORY https://github.com/KonanM/CppProperties.git
  GIT_TAG e18f5c648b7a085b1a8454c8f07ece31737d620a
)
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(CppProperties)

find_package(CURL QUIET)
if(NOT CURL_FOUND)
    FetchContent_Declare(
        curl
        URL https://curl.se/download/curl-8.21.0.tar.xz
        OVERRIDE_FIND_PACKAGE
    )
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
    set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    set(CURL_WINDOWS_SSPI ON CACHE BOOL "" FORCE)
    if(UNIX AND NOT APPLE)
        set(OPENSSL_USE_STATIC_LIBS TRUE CACHE BOOL "" FORCE)
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    endif()
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(curl)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti")
endif()

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

function(spiration_setup_runtime target)
    if(MINGW)
        get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_EXE=$<TARGET_FILE:${target}>
            -DOUTPUT_DIR=$<TARGET_FILE_DIR:${target}>
            -DMINGW_BIN_DIR=${MINGW_BIN_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeDeps.cmake
    )
    if(APPLE)
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN")
    endif()
endfunction()

add_subdirectory(common)
if(WIN32)
    add_subdirectory(windows)
elseif(APPLE)
    add_subdirectory(macos)
elseif(UNIX)
    add_subdirectory(linux)
endif()

install(TARGETS spiration-common
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

install(TARGETS spiration
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION bin
)