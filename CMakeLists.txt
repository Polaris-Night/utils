cmake_minimum_required(VERSION 3.14)

project(utils)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(BuildBreakpad)
build_breakpad(${CMAKE_SOURCE_DIR}/third_party/breakpad)

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(test)
