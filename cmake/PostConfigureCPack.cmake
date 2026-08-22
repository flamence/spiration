install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)
if(WIN32)
    set_property(INSTALL "bin/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
                 PROPERTY CPACK_DESKTOP_SHORTCUTS "${SPIRATION_APP_NAME}")
    set_property(INSTALL "bin/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
                 PROPERTY CPACK_START_MENU_SHORTCUTS "${SPIRATION_APP_NAME}")
endif()
if(APPLE)
    install(CODE "
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${SPIRATION_RESOURCE_SRC_DIR}
                \${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app/Contents/Resources/lang
        )
    ")
    install(CODE "
        execute_process(
            COMMAND codesign --force --deep --sign - 
                \${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app
        )
    ")
else()
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/common/lang DESTINATION "${SPIRATION_INSTALL_RESOURCES}")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    SKIP_BUILD_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$ORIGIN/../lib"
    INSTALL_RPATH "@loader_path/../lib"
    BUILD_WITH_INSTALL_RPATH FALSE
)

if(MINGW_DIRECTORY)
    foreach(_dll libstdc++-6.dll libwinpthread-1.dll
                    libgcc_s_seh-1.dll libgcc_s_sjlj-1.dll libgcc_s_dw2-1.dll)
        if(EXISTS "${MINGW_DIRECTORY}/${_dll}")
            install(FILES "${MINGW_DIRECTORY}/${_dll}" DESTINATION lib)
        endif()
    endforeach()
endif()

include(CPack)
