cmake_minimum_required(VERSION 3.18)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
endif()

project(spiration VERSION 0.84 LANGUAGES C CXX)

include(cmake/ConfigureBranding.cmake)
include(cmake/ConfigureCXX.cmake)
include(cmake/PreConfigureCPack.cmake)
include(cmake/ConfigurePlatforms.cmake)
include(cmake/PostConfigureCPack.cmake)
