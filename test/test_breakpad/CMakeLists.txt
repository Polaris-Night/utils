add_library(crash_lib SHARED crash_lib.cpp crash_lib.h)
add_executable(test_crash test_crash.cpp)

utils_apply_warnings(crash_lib)
utils_apply_warnings(test_crash)

# utils 已 PUBLIC 导出头文件目录；crash_lib.h 与源码同目录，无需额外 include 路径
target_link_libraries(test_crash PRIVATE utils crash_lib)
