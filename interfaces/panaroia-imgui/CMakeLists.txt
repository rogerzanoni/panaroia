cmake_minimum_required(VERSION 3.17)

set(TARGET_NAME "panaroia-imgui")

project(${TARGET_NAME} VERSION 0.1
                 DESCRIPTION "sample gui for panaroia"
                 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/../../include)
include_directories(${PROJECT_SOURCE_DIR}/../../3rdparty/imgui-filebrowser)

message(${PROJECT_SOURCE_DIR}/3rdparty/imgui-filebrowser)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

include_directories(${CONAN_INCLUDE_DIRS_IMGUI})
include_directories(${CMAKE_BINARY_DIR}/interfaces/imgui-bindings)

set(SOURCES
    main.cpp
    ${CMAKE_BINARY_DIR}/interfaces/imgui-bindings/imgui_impl_sdl.cpp
    ${CMAKE_BINARY_DIR}/interfaces/imgui-bindings/imgui_impl_opengl3.cpp
    panaroiacontroller.cpp
)

add_executable(${TARGET_NAME} ${SOURCES})

target_link_libraries(${TARGET_NAME}
    panaroia
    stdc++fs
    OpenGL::GL
    ${GLEW_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${CONAN_LIBS}
)
