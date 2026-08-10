if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME} ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT "UDBZ")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Spiration")
    set(CPACK_NSIS_MODIFY_PATH "ON")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "Spiration.exe")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        CreateShortCut '\$SMPROGRAMS\\\\\$STARTMENU_FOLDER\\\\Spiration.lnk' '\$INSTDIR\\\\Spiration.exe'
        StrCmp \$INSTALL_DESKTOP '1' 0 +2
        CreateShortCut '\$DESKTOP\\\\Spiration.lnk' '\$INSTDIR\\\\Spiration.exe'
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        ReadRegStr \$R0 SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Spiration' 'StartMenu'
        StrCmp \$R0 '' 0 +2
        StrCpy \$R0 'Spiration'
        Delete '\$SMPROGRAMS\\\\\$R0\\\\Spiration.lnk'
        Delete '\$DESKTOP\\\\Spiration.lnk'
    ")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()
set(CPACK_PACKAGE_CONTACT "@")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pkg")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-v${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Spiration")
set(CPACK_PACKAGE_VENDOR "Flamence Tech.")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
