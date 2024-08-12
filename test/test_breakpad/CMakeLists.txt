add_library(crash_lib SHARED crash_lib.cpp crash_lib.h)
add_executable(test_crash test_crash.cpp)
target_link_libraries(test_crash PRIVATE utils crash_lib)
target_include_directories(test_crash PRIVATE ${UTILS_INCLUDES_DIR} ${CMAKE_CURRENT_LIST_DIR})