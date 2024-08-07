cmake_minimum_required(VERSION 3.14)

project(utils)

set(CXX_STANDARD 17 REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
add_subdirectory(third_party)
add_subdirectory(test)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE STRING "${CMAKE_INSTALL_PREFIX}" FORCE)
