cmake_minimum_required(VERSION 4.1)

project(stb_lib)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(${PROJECT_NAME} STATIC)

target_sources(
        ${PROJECT_NAME}
        PRIVATE
        stb_image_impl.cpp
        stb_truetype_impl.cpp
)

target_include_directories(
        ${PROJECT_NAME}
        PUBLIC
        .
)