file(GLOB_RECURSE WINDOWS_SOURCES 
    "src/*.cpp" 
    "src/*.c"
)
file(GLOB_RECURSE WINDOWS_HEADERS 
    "include/*.h" 
    "include/*.hpp"
)
set(WINDOWS_RESOURCES
    res/resources.rc
)

add_executable(${PROJECT_NAME} ${WINDOWS_SOURCES} ${WINDOWS_HEADERS} ${WINDOWS_RESOURCES})

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
)

if(WINDOWS_SOURCES MATCHES "\\.rc$")
    set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff <DEFINES> <INCLUDES> <FLAGS> <SOURCE> <OBJECT>")
endif()

if(MINGW)
    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)

    set(MINGW_RUNTIME_DLLS
        "${MINGW_BIN_DIR}/libstdc++-6.dll"
        "${MINGW_BIN_DIR}/libwinpthread-1.dll"
    )

    foreach(_gcc_dll libgcc_s_seh-1.dll libgcc_s_sjlj-1.dll libgcc_s_dw2-1.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${_gcc_dll}")
            list(APPEND MINGW_RUNTIME_DLLS "${MINGW_BIN_DIR}/${_gcc_dll}")
            break()
        endif()
    endforeach()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MINGW_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/lang
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/lang
)

set_target_properties(spiration PROPERTIES
    OUTPUT_NAME "Spiration"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../bin
    WIN32_EXECUTABLE $<IF:$<CONFIG:Debug>,FALSE,TRUE>
)