if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)
FetchContent_Declare(
    spiration
    SOURCE_DIR ${SPIRATION_ROOT_PATH}/common
)
FetchContent_MakeAvailable(spiration)
