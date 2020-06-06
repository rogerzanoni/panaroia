cmake_minimum_required(VERSION 3.17)

set(TARGET_NAME "panaroia-tests")

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})
link_directories(${CHECK_LIBRARY_DIRS})

add_executable(${TARGET_NAME} panaroia_tests.c)

include_directories(${PROJECT_SOURCE_DIR}/include)

set_property(TARGET ${TARGET_NAME} PROPERTY C_STANDARD 11)

target_link_libraries(${TARGET_NAME} panaroia ${CHECK_LIBRARIES} m pthread rt subunit)

add_test(panaroia_test ${TARGET_NAME})
