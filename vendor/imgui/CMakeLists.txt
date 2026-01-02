cmake_minimum_required(VERSION 4.1)

project(imgui)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC)

target_sources(
        ${PROJECT_NAME}
        PRIVATE
        imgui.cpp
        imgui_demo.cpp
        imgui_draw.cpp
        imgui_tables.cpp
        imgui_widgets.cpp
        backends/imgui_impl_sdl3.cpp
        backends/imgui_impl_vulkan.cpp
)

target_include_directories(
        ${PROJECT_NAME}
        PUBLIC
        .
)

target_compile_definitions(
        imgui
        PUBLIC
)