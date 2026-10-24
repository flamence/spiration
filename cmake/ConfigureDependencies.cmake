if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

include(FetchContent)

set(BUILD_TESTING OFF)

FetchContent_Declare(
    spiration.api
    GIT_REPOSITORY https://github.com/flamence/spiration.api.git
    GIT_TAG main
)
FetchContent_MakeAvailable(spiration.api)

FetchContent_Declare(
    fmt
    URL https://codeload.github.com/fmtlib/fmt/tar.gz/refs/tags/10.2.1
)
FetchContent_GetProperties(fmt)
if(NOT fmt_POPULATED)
    FetchContent_Populate(fmt)
endif()
if(NOT TARGET fmt::fmt)
    add_library(fmt::fmt INTERFACE IMPORTED)
    set_target_properties(fmt::fmt PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${fmt_SOURCE_DIR}/include"
        INTERFACE_COMPILE_DEFINITIONS "FMT_HEADER_ONLY"
    )
endif()

FetchContent_Declare(
    ghc_filesystem
    URL https://codeload.github.com/gulrak/filesystem/tar.gz/refs/tags/v1.5.14
)
FetchContent_GetProperties(ghc_filesystem)
if(NOT ghc_filesystem_POPULATED)
    FetchContent_Populate(ghc_filesystem)
endif()
if(NOT TARGET ghc::filesystem)
    add_library(ghc::filesystem INTERFACE IMPORTED)
    set_target_properties(ghc::filesystem PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ghc_filesystem_SOURCE_DIR}/include"
    )
endif()

FetchContent_Declare(
    tl_optional
    URL https://codeload.github.com/TartanLlama/optional/tar.gz/refs/tags/v1.1.0
)
FetchContent_GetProperties(tl_optional)
if(NOT tl_optional_POPULATED)
    FetchContent_Populate(tl_optional)
endif()
if(NOT TARGET tl::optional)
    add_library(tl::optional INTERFACE IMPORTED)
    set_target_properties(tl::optional PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${tl_optional_SOURCE_DIR}/include"
    )
endif()

FetchContent_Declare(
    nonstd_string_view
    URL https://codeload.github.com/nonstd-lite/string-view-lite/tar.gz/refs/tags/v1.7.0
)
FetchContent_GetProperties(nonstd_string_view)
if(NOT nonstd_string_view_POPULATED)
    FetchContent_Populate(nonstd_string_view)
endif()
if(NOT TARGET nonstd::string_view)
    add_library(nonstd::string_view INTERFACE IMPORTED)
    set_target_properties(nonstd::string_view PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${nonstd_string_view_SOURCE_DIR}/include"
    )
endif()

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

if (NOT OHOS)
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
            set(OPENSSL_USE_STATIC_LIBS FALSE CACHE BOOL "" FORCE)
            set(CURL_USE_OPENSSL    ON CACHE BOOL "" FORCE)
        endif()
        set(BUILD_CURL_EXE        OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTING         OFF CACHE BOOL "" FORCE)
        set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(curl)
    endif()
endif()

FetchContent_Declare(
    nothings_stb_image
    URL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_NAME stb_image.h
)
FetchContent_MakeAvailable(nothings_stb_image)

if(NOT TARGET nothings::stb_image)
    add_library(nothings::stb_image INTERFACE IMPORTED)
    set_target_properties(nothings::stb_image PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${nothings_stb_image_SOURCE_DIR}"
    )
endif()
