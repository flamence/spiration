set(SPIRATION_CORE_SOURCES
    src/application.cpp
    src/extension/extension.cpp
    src/extension/extension_api.cpp
    src/extension/extension_manager.cpp
    src/extension/extension_manifest.cpp
    src/extension/manifest_extension.cpp
    src/extension/builtin/agent/agent_tab.cpp
    src/extension/builtin/agent/chat.cpp
    src/extension/builtin/agent/chat_store.cpp
    src/extension/builtin/agent/extension.cpp
    src/extension/builtin/agent/provider.cpp
    src/extension/builtin/agent/registry.cpp
    src/extension/builtin/agent/tool/edit_tool.cpp
    src/extension/builtin/agent/tool/memory_tool.cpp
    src/extension/builtin/agent/tool/sleep_tool.cpp
    src/extension/builtin/agent/tool/terminal_tool.cpp
    src/extension/builtin/agent/tool/todo_tool.cpp
    src/extension/builtin/agent/tool/web_tool.cpp
    src/extension/builtin/agent/ui/todo_view.cpp
    src/extension/builtin/edit/edit_tab.cpp
    src/extension/builtin/edit/extension.cpp
    src/extension/builtin/i18n/extension.cpp
    src/extension/builtin/i18n/i18n.cpp
    src/extension/builtin/theme/extension.cpp
    src/ui/about_tab.cpp
    src/ui/appbar.cpp
    src/ui/button.cpp
    src/ui/checkbox.cpp
    src/ui/collapsible.cpp
    src/ui/combo_box.cpp
    src/ui/container.cpp
    src/ui/context_menu.cpp
    src/ui/cursor.cpp
    src/ui/dialog.cpp
    src/ui/extension_tab.cpp
    src/ui/focus_manager.cpp
    src/ui/input_dialog.cpp
    src/ui/label.cpp
    src/ui/layout.cpp
    src/ui/list_view.cpp
    src/ui/markdown.cpp
    src/ui/menu_bar.cpp
    src/ui/popup_menu.cpp
    src/ui/progress_bar.cpp
    src/ui/radio_button.cpp
    src/ui/root.cpp
    src/ui/scroll_row.cpp
    src/ui/separator.cpp
    src/ui/slider.cpp
    src/ui/split_pane.cpp
    src/ui/tab_bar.cpp
    src/ui/text_area.cpp
    src/ui/text_field.cpp
    src/ui/theme_manager.cpp
    src/ui/toggle_switch.cpp
    src/ui/tooltip.cpp
    src/ui/widget.cpp
    src/ui/window_controls.cpp
    src/utils/console.cpp
    src/utils/crash_log.cpp
    src/utils/io.cpp
    src/utils/platform.cpp
    src/utils/properties.cpp
)

if(UNIX AND NOT APPLE)
    list(APPEND SPIRATION_CORE_SOURCES src/renderer/opengl_renderer.cpp)
endif()

file(GLOB_RECURSE SPIRATION_CORE_HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "include/*.hpp"
)

add_library(spiration.core STATIC ${SPIRATION_CORE_HEADERS} ${SPIRATION_CORE_SOURCES})

set_target_properties(spiration.core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(spiration.core PRIVATE
    nlohmann_json::nlohmann_json
    CURL::libcurl
    stb::stb
)

if(WIN32)
    target_link_libraries(spiration.core PRIVATE dbghelp)
endif()

if(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    find_package(Freetype REQUIRED)
    target_include_directories(spiration.core PRIVATE
        ${OPENGL_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})
    target_link_libraries(spiration.core PRIVATE
        ${OPENGL_LIBRARIES} GLX ${FREETYPE_LIBRARIES})
endif()

target_include_directories(spiration.core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(spiration.core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(spiration::core ALIAS spiration.core)

add_library(spiration.api INTERFACE)

target_include_directories(spiration.api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_library(spiration::api ALIAS spiration.api)
