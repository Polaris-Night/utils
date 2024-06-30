add_subdirectory(googletest)

add_executable(test main.cpp)
target_link_libraries(test PRIVATE utils gtest)
target_include_directories(test PRIVATE ${UTILS_INCLUDES_DIR})