add_subdirectory(googletest)

set(TEST_SRC
    test_stringutil.cpp
    CACHE INTERNAL "" FORCE
)

add_executable(main main.cpp ${TEST_SRC})
target_link_libraries(main PRIVATE utils gtest)
target_include_directories(main PRIVATE ${UTILS_INCLUDES_DIR})