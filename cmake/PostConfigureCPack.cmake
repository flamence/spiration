set_property(INSTALL "$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    PROPERTY CPACK_DESKTOP_SHORTCUTS "Spiration"
)
set_property(INSTALL "$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    PROPERTY CPACK_START_MENU_SHORTCUTS "Spiration"
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/common/lang DESTINATION ".")

if(MINGW_DIRECTORY)
    foreach(_dll libstdc++-6.dll libwinpthread-1.dll
                    libgcc_s_seh-1.dll libgcc_s_sjlj-1.dll libgcc_s_dw2-1.dll)
        if(EXISTS "${MINGW_DIRECTORY}/${_dll}")
            install(FILES "${MINGW_DIRECTORY}/${_dll}" DESTINATION ".")
        endif()
    endforeach()
endif()

include(CPack)
