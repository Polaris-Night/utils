cmake_minimum_required(VERSION 3.14)

project(breakpad)

if (WIN32)
    add_definitions(-D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
                    -DUNICODE
                    -D_UNICODE)
elseif(UNIX)
    add_definitions(-DHAVE_A_OUT_H)
else()
    message(FATAL_ERROR "Not support system ${CMAKE_SYSTEM_NAME}")
endif()

add_subdirectory(src)

install(
    DIRECTORY
    src/client
    src/common
    src/google_breakpad
    src/processor
    src/third_party
    DESTINATION include/breakpad
    FILES_MATCHING PATTERN "*.h"
)