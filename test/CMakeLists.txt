add_subdirectory(googletest)

file(GLOB TEST_SRC ${CMAKE_CURRENT_LIST_DIR}/test_*.cpp)

add_executable(main main.cpp ${TEST_SRC})
target_link_libraries(main PRIVATE utils gtest)
target_include_directories(main PRIVATE ${UTILS_INCLUDES_DIR})

add_subdirectory(test_breakpad)