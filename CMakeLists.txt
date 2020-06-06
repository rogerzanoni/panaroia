cmake_minimum_required(VERSION 3.17)

set(TARGET_NAME "panaroia")

project(${TARGET_NAME} VERSION 0.1
                 DESCRIPTION "a small chip8 interpreter"
                 LANGUAGES C)

set(SOURCES
    src/panaroia.c
    ${PROJECT_SOURCE_DIR}/3rdparty/log.c/src/log.c
)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/3rdparty/log.c/src
)

add_library(${TARGET_NAME} SHARED ${SOURCES})
set_property(TARGET ${TARGET_NAME} PROPERTY C_STANDARD 11)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(ENABLE_TESTS "Enable unit testing" ON)

if (ENABLE_TESTS)
    include(CheckCSourceCompiles)
    include(CheckCSourceRuns)
    include(CheckFunctionExists)
    include(CheckIncludeFile)
    include(CheckIncludeFiles)
    include(CheckLibraryExists)
    include(CheckSymbolExists)
    include(CheckTypeSize)

    set(INCLUDES "")
    macro(ck_check_include_file header var)
        check_include_files("${INCLUDES};${header}" ${var})
        if(${var})
            set(INCLUDES ${INCLUDES} ${header})
        endif(${var})
    endmacro(ck_check_include_file)

    ck_check_include_file("stdlib.h" HAVE_STDLIB_H)

    check_type_size(intmax_t INTMAX_T)
    check_type_size(uintmax_t UINTMAX_T)

    check_type_size(pid_t PID_T)
    if(NOT HAVE_PID_T)
        if(WIN32)
            set(pid_t "int")
        else(WIN32)
            MESSAGE(FATAL_ERROR "pid_t doesn't exist on this platform?")
        endif(WIN32)
    endif(NOT HAVE_PID_T)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/config.h)
    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
    add_definitions(-DHAVE_CONFIG_H)
    set(CONFIG_HEADER ${CMAKE_CURRENT_BINARY_DIR}/config.h)

    enable_testing()
    add_subdirectory(tests)
endif(ENABLE_TESTS)

option(ENABLE_GUI "Build sample gui" ON)
if (ENABLE_GUI)
    add_subdirectory(interfaces/panaroia-imgui)

    file(COPY roms DESTINATION ${PROJECT_BINARY_DIR})
endif (ENABLE_GUI)
