if(NOT EXISTS "${TARGET_EXE}")
    return()
endif()

if(WIN32)
    return()
endif()

if(NOT EXISTS "${OUTPUT_DIR}")
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")
endif()

if(APPLE)
    execute_process(COMMAND otool -L "${TARGET_EXE}"
                    OUTPUT_VARIABLE OTOOL_OUT RESULT_VARIABLE OTOOL_RC)
    if(NOT OTOOL_RC EQUAL 0)
        return()
    endif()
    string(REPLACE "\n" ";" LINES "${OTOOL_OUT}")
    foreach(line IN LISTS LINES)
        string(STRIP "${line}" line)
        if(line MATCHES "^/([^ ]+\\.dylib)")
            set(lib "/${CMAKE_MATCH_1}")
            if(NOT lib MATCHES "^/System/" AND
               NOT lib MATCHES "^/usr/lib/" AND
               NOT lib MATCHES "^/Library/" AND
               NOT lib MATCHES "^/usr/local/")
                file(COPY "${lib}" DESTINATION "${OUTPUT_DIR}")
            endif()
        endif()
    endforeach()
else()
    execute_process(COMMAND ldd "${TARGET_EXE}"
                    OUTPUT_VARIABLE LDD_OUT RESULT_VARIABLE LDD_RC)
    if(NOT LDD_RC EQUAL 0)
        return()
    endif()
    string(REPLACE "\n" ";" LINES "${LDD_OUT}")
    foreach(line IN LISTS LINES)
        string(STRIP "${line}" line)
        if(line MATCHES "=> (/[^ ]+)")
            set(lib "${CMAKE_MATCH_1}")
            if(NOT lib MATCHES "^/usr/" AND
               NOT lib MATCHES "^/lib/" AND
               NOT lib MATCHES "^/lib64/")
                file(COPY "${lib}" DESTINATION "${OUTPUT_DIR}")
            endif()
        endif()
    endforeach()
endif()
