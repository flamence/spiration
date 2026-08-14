if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

set(BUILD_TESTING OFF)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

find_package(CURL QUIET)
if(NOT CURL_FOUND)
    FetchContent_Declare(
        curl
        URL https://curl.se/download/curl-8.21.0.tar.xz
        OVERRIDE_FIND_PACKAGE
    )
    set(CURL_USE_LIBPSL       OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2      OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB             OFF CACHE BOOL "" FORCE)
    set(CURL_ENABLE_SSL        ON CACHE BOOL "" FORCE)
    if(WIN32)
        set(CURL_USE_SCHANNEL      ON CACHE BOOL "" FORCE)
        set(CURL_WINDOWS_SSPI      ON CACHE BOOL "" FORCE)
    endif()
    if(UNIX AND NOT APPLE)
        set(OPENSSL_USE_STATIC_LIBS TRUE CACHE BOOL "" FORCE)
        set(CURL_USE_OPENSSL    ON CACHE BOOL "" FORCE)
    endif()
    set(BUILD_CURL_EXE        OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING         OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(curl)
endif()

FetchContent_Declare(
    stb
    URL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_NAME stb_image.h
)
FetchContent_MakeAvailable(stb)

if(NOT TARGET stb::stb)
    add_library(stb::stb INTERFACE IMPORTED)
    set_target_properties(stb::stb PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${stb_SOURCE_DIR}"
    )
endif()
