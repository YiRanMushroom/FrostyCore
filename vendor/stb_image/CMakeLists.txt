cmake_minimum_required(VERSION 4.1)

project(stb_image)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC)

target_sources(
        ${PROJECT_NAME}
        PRIVATE
        stb_image_impl.cpp
)

target_include_directories(
        ${PROJECT_NAME}
        PUBLIC
        .
)