set(COMMON_SRC_DIR)
set(COMMON_LIB breakpad_common)
set(CLIENT_SRC_DIR)
set(CLIENT_LIB breakpad_client)

add_subdirectory(third_party)

if (WIN32)
    set(COMMON_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/common/windows)
    set(CLIENT_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/client/windows)
elseif(UNIX)
    set(COMMON_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/common/linux)
    set(CLIENT_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/client/linux)
else()
    message(FATAL_ERROR "Not support system ${CMAKE_SYSTEM_NAME}")
endif()

# common
file(GLOB COMMON_SRC
    ${CMAKE_CURRENT_LIST_DIR}/common/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/*.h
    ${COMMON_SRC_DIR}/*.cc
    ${COMMON_SRC_DIR}/*.h
    ${COMMON_SRC_DIR}/*.S
)
file(GLOB COMMON_TEST_SRC
    ${CMAKE_CURRENT_LIST_DIR}/common/*test.cc
    ${COMMON_SRC_DIR}/*test.cc
)
list(REMOVE_ITEM COMMON_SRC ${COMMON_TEST_SRC})
add_library(${COMMON_LIB} STATIC ${COMMON_SRC})
target_include_directories(${COMMON_LIB} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# client
file(GLOB_RECURSE CLIENT_SRC
    ${CLIENT_SRC_DIR}/*.cc
    ${CLIENT_SRC_DIR}/*.h
)
file(GLOB CLIENT_INDEPENDENCE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/client/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/client/*.h
)
list(APPEND CLIENT_SRC ${CLIENT_INDEPENDENCE_SRC})
file(GLOB_RECURSE CLIENT_TEST_SRC ${CMAKE_CURRENT_LIST_DIR}/client/*test.cc)
list(REMOVE_ITEM CLIENT_SRC ${CLIENT_TEST_SRC} ${CLIENT_SRC_DIR}/sender/google_crash_report_sender.cc)
add_library(${CLIENT_LIB} STATIC ${CLIENT_SRC})
target_link_libraries(${CLIENT_LIB} PUBLIC ${COMMON_LIB})
target_include_directories(${CLIENT_LIB} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

install(
    TARGETS ${CLIENT_LIB}
    ARCHIVE DESTINATION lib
)
