include(../cmake/ConfigureDependencies.cmake)

set(SPIRATION_CORE_SOURCES
    src/spiration/api.cpp
    src/spiration/application.cpp
    src/spiration/extension/extension_api.cpp
    src/spiration/extension/extension_manager.cpp
    src/spiration/extension/extension_manifest.cpp
    src/spiration/extension/builtin/agent/agent_tab.cpp
    src/spiration/extension/builtin/agent/chat.cpp
    src/spiration/extension/builtin/agent/chat_store.cpp
    src/spiration/extension/builtin/agent/extension.cpp
    src/spiration/extension/builtin/agent/registry.cpp
    src/spiration/extension/builtin/agent/web_provider.cpp
    src/spiration/extension/builtin/agent/tool/edit_tool.cpp
    src/spiration/extension/builtin/agent/tool/memory_tool.cpp
    src/spiration/extension/builtin/agent/tool/sleep_tool.cpp
    src/spiration/extension/builtin/agent/tool/terminal_tool.cpp
    src/spiration/extension/builtin/agent/tool/todo_tool.cpp
    src/spiration/extension/builtin/agent/tool/web_tool.cpp
    src/spiration/extension/builtin/agent/ui/todo_view.cpp
    src/spiration/extension/builtin/edit/edit_tab.cpp
    src/spiration/extension/builtin/edit/extension.cpp
    src/spiration/extension/builtin/i18n/extension.cpp
    src/spiration/extension/builtin/i18n/i18n.cpp
    src/spiration/extension/builtin/theme/extension.cpp
    src/spiration/logging/sink/console.cpp
    src/spiration/logging/sink/file.cpp
    src/spiration/logging/logger.cpp
    src/spiration/net/http.cpp
    src/spiration/renderer/opengl_renderer.cpp
    src/spiration/ui/about_tab.cpp
    src/spiration/ui/appbar.cpp
    src/spiration/ui/button.cpp
    src/spiration/ui/checkbox.cpp
    src/spiration/ui/collapsible.cpp
    src/spiration/ui/combo_box.cpp
    src/spiration/ui/container.cpp
    src/spiration/ui/context_menu.cpp
    src/spiration/ui/cursor.cpp
    src/spiration/ui/dialog.cpp
    src/spiration/ui/extension_tab.cpp
    src/spiration/ui/focus_manager.cpp
    src/spiration/ui/input_dialog.cpp
    src/spiration/ui/label.cpp
    src/spiration/ui/layout.cpp
    src/spiration/ui/list_view.cpp
    src/spiration/ui/markdown.cpp
    src/spiration/ui/menu_bar.cpp
    src/spiration/ui/popup_menu.cpp
    src/spiration/ui/progress_bar.cpp
    src/spiration/ui/radio_button.cpp
    src/spiration/ui/root.cpp
    src/spiration/ui/scroll_row.cpp
    src/spiration/ui/separator.cpp
    src/spiration/ui/slider.cpp
    src/spiration/ui/split_pane.cpp
    src/spiration/ui/tab_bar.cpp
    src/spiration/ui/tab_head_item.cpp
    src/spiration/ui/text_area.cpp
    src/spiration/ui/text_field.cpp
    src/spiration/ui/theme_manager.cpp
    src/spiration/ui/toggle_switch.cpp
    src/spiration/ui/tooltip.cpp
    src/spiration/ui/widget.cpp
    src/spiration/ui/window_controls.cpp
    src/spiration/window/ime.cpp
    src/spiration/utils/crash_log.cpp
    src/spiration/utils/dispatcher.cpp
    src/spiration/utils/io.cpp
    src/spiration/utils/platform.cpp
    src/spiration/utils/properties.cpp
)

if (NOT OHOS)
    list(APPEND SPIRATION_CORE_SOURCES
        src/spiration/net/curl_http.cpp
    )
else()
    list(FILTER SPIRATION_CORE_SOURCES EXCLUDE REGEX
        ".*/extension/builtin/agent/tool/terminal_tool\\.cpp$")
    list(FILTER SPIRATION_CORE_SOURCES EXCLUDE REGEX
        ".*/utils/platform\\.cpp$")
endif()

file(GLOB_RECURSE SPIRATION_CORE_HEADERS CONFIGURE_DEPENDS
    "include/*.h"
    "include/*.hpp"
)

add_library(spiration.core STATIC ${SPIRATION_CORE_HEADERS} ${SPIRATION_CORE_SOURCES})

target_link_libraries(spiration.core PUBLIC
    fmt::fmt
    ghc::filesystem
    nlohmann_json::nlohmann_json
    nonstd::string_view
    nothings::stb_image
    spiration::api
    tl::optional
)

if (NOT OHOS)
    target_link_libraries(spiration.core PUBLIC
        CURL::libcurl
    )
endif()

if(WIN32)
    target_link_libraries(spiration.core PRIVATE dbghelp)
endif()

if(UNIX AND NOT APPLE AND NOT OHOS)
    find_package(OpenGL REQUIRED)
    find_package(Freetype REQUIRED)
    target_include_directories(spiration.core PRIVATE
        ${OPENGL_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})
    target_link_libraries(spiration.core PRIVATE
        ${OPENGL_LIBRARIES} GLX ${FREETYPE_LIBRARIES})
elseif(APPLE)
    find_library(OPENGL_LIBRARY OpenGL)
    target_link_libraries(spiration.core PRIVATE ${OPENGL_LIBRARY})
    target_compile_definitions(spiration.core PRIVATE GL_SILENCE_DEPRECATION)
endif()

target_include_directories(spiration.core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spiration
)

target_include_directories(spiration.core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(spiration::core ALIAS spiration.core)
