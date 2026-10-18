cmake_minimum_required(VERSION 3.14)

project(utils VERSION 1.0.0 LANGUAGES C CXX)

# ==================== 选项 ====================

option(UTILS_WERROR               "把编译告警当作错误"                                        OFF)
option(UTILS_BUILD_BREAKPAD_TOOLS "构建 breakpad 符号化工具（dump_syms 等，耗时且多数场景用不到）" OFF)
option(UTILS_ENABLE_SANITIZERS    "启用 sanitizer（见 UTILS_SANITIZER，开发/CI 用）"            OFF)
option(UTILS_ENABLE_CCACHE        "自动探测并使用 ccache 加速编译"                            ON)
set(UTILS_SANITIZER "address,undefined" CACHE STRING "sanitizer 列表，如 address,undefined / thread / address")

# 单配置生成器未指定构建类型时给默认值（否则既无优化也无调试信息）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "构建类型" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ==================== 语言与全局设置 ====================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# ==================== 编译告警 ====================

set(UTILS_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

# 给本工程自己的目标套用统一告警（第三方目标 breakpad/gtest 不套用）
function(utils_apply_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE ${UTILS_WARNING_FLAGS})
        if(UTILS_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ==================== Sanitizer（默认关闭）====================

if(UTILS_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=${UTILS_SANITIZER} -fno-omit-frame-pointer)
        add_link_options(-fsanitize=${UTILS_SANITIZER})
        message(STATUS "utils: 已启用 sanitizer: ${UTILS_SANITIZER}")
    else()
        message(WARNING "utils: UTILS_ENABLE_SANITIZERS 仅支持 GCC/Clang，已忽略")
    endif()
endif()

# ==================== ccache（可选加速）====================

if(UTILS_ENABLE_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER AND NOT CMAKE_C_COMPILER_LAUNCHER)
    find_program(UTILS_CCACHE_EXECUTABLE ccache)
    if(UTILS_CCACHE_EXECUTABLE)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${UTILS_CCACHE_EXECUTABLE} CACHE STRING "C++ 编译启动器" FORCE)
        set(CMAKE_C_COMPILER_LAUNCHER ${UTILS_CCACHE_EXECUTABLE} CACHE STRING "C 编译启动器" FORCE)
        message(STATUS "utils: 已启用 ccache（${UTILS_CCACHE_EXECUTABLE}）")
    endif()
endif()

# ==================== 内容 ====================

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(BuildBreakpad)
build_breakpad(${CMAKE_CURRENT_SOURCE_DIR}/third_party/breakpad)

add_subdirectory(third_party)
add_subdirectory(src)

# CTest：提供 BUILD_TESTING 选项与 enable_testing()
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
