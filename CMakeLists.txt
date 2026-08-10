cmake_minimum_required(VERSION 3.16)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
endif()

project(spiration VERSION 0.83 LANGUAGES C CXX)

include(cmake/ConfigureCXX.cmake)
include(cmake/ConfigureDependencies.cmake)
include(cmake/PreConfigureCPack.cmake)
include(cmake/ConfigurePlatforms.cmake)
include(cmake/PostConfigureCPack.cmake)
