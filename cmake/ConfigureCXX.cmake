set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MINGW)
    get_filename_component(MINGW_DIRECTORY ${CMAKE_CXX_COMPILER} DIRECTORY)
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

if(MSVC)
    add_compile_options(/EHsc)
else()
    add_compile_options(-fexceptions)
endif()

if(MSVC)
    add_compile_options("/utf-8")
endif()
