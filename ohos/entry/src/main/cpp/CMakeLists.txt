cmake_minimum_required(VERSION 3.5.0)
project(spiration)

include(cmake/ConfigureCXX.cmake)
include(cmake/ConfigureVariables.cmake)
include(${SPIRATION_ROOT_PATH}/cmake/ConfigureLocal.cmake OPTIONAL)
include(cmake/ConfigureDependencies.cmake)
include(cmake/ConfigureLibrary.cmake)
