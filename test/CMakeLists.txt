# googletest：只构建 gtest，且不安装（本工程不用 gmock）
set(INSTALL_GTEST OFF CACHE BOOL "googletest: 不安装" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "googletest: 不构建 gmock" FORCE)
if(MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "googletest: 使用共享 CRT" FORCE)
endif()
add_subdirectory(googletest)

file(GLOB TEST_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/test_*.cpp)

add_executable(main main.cpp ${TEST_SRC})
# utils 已 PUBLIC 导出头文件目录，无需再显式 include
target_link_libraries(main PRIVATE utils gtest)
utils_apply_warnings(main)

# 注册到 CTest：支持 ctest / ctest -j / 单测重跑（二进制仍可直接运行）
include(GoogleTest)
gtest_discover_tests(main)

add_subdirectory(test_breakpad)
