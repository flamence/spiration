file(GLOB_RECURSE WINDOWS_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.c"
)
file(GLOB_RECURSE WINDOWS_HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "include/*.hpp"
)
set(WINDOWS_RESOURCES
    res/resources.rc
)

add_executable(${PROJECT_NAME} WIN32 ${WINDOWS_SOURCES} ${WINDOWS_HEADERS} ${WINDOWS_RESOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/res
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spiration-common
    d2d1
    dwrite
    windowscodecs
    winmm
    ole32
    dwmapi
    comdlg32
    imm32
    winhttp
    dbghelp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "Spiration"
    WIN32_EXECUTABLE TRUE
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/lang
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/lang
)

if(MINGW_DIRECTORY)
    set(_mingw_dlls)
    foreach(_dll libstdc++-6.dll libwinpthread-1.dll
                 libgcc_s_seh-1.dll libgcc_s_sjlj-1.dll libgcc_s_dw2-1.dll)
        if(EXISTS "${MINGW_DIRECTORY}/${_dll}")
            list(APPEND _mingw_dlls "${MINGW_DIRECTORY}/${_dll}")
        endif()
    endforeach()
    if(_mingw_dlls)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_mingw_dlls}
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
endif()

if(MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-all-symbols)
endif()
