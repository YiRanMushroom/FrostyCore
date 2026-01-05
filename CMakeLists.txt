cmake_minimum_required(VERSION 4.1)
project(FrostyCore)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC)

file(GLOB_RECURSE IMPL_UNITS "src/*.cpp")
file(GLOB_RECURSE INTERFACE_UNITS "src/*.ixx" "src/*.cppm")

target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(
        ${PROJECT_NAME}
        PRIVATE
        ${IMPL_UNITS}
)

target_sources(
        ${PROJECT_NAME}
        PRIVATE
        FILE_SET cxx_modules
        TYPE CXX_MODULES FILES
        ${INTERFACE_UNITS}
)

target_compile_definitions(
        ${PROJECT_NAME}
        PUBLIC
        "Engine=Frosty"
)

include(cmake/GenerateShaderSource.cmake)
add_shader_embedding_target(ShaderBuildTarget
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/shaders"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/GeneratedShaders.cppm"
)

set(NVRHI_BUILD_SHARED OFF CACHE BOOL "Do not build NVRHI as a shared library" FORCE)
set(NVRHI_WITH_VALIDATION ON CACHE BOOL "Build NVRHI with validation layers" FORCE)
set(NVRHI_WITH_VULKAN ON CACHE BOOL "Build NVRHI with Vulkan backend" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build dependencies as static libraries" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Do not build SDL as a shared library" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "Disable SDL audio subsystem" FORCE)
set(SDL_VIDEO ON CACHE BOOL "Disable SDL video subsystem" FORCE)
set(SDL_GPU OFF CACHE BOOL "Disable SDL GPU subsystem" FORCE)
set(SDL_RENDER OFF CACHE BOOL "Disable SDL render subsystem" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "Disable SDL camera subsystem" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "Disable SDL joystick subsystem" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "Disable SDL haptic subsystem" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "Disable SDL HIDAPI subsystem" FORCE)
set(SDL_POWER OFF CACHE BOOL "Disable SDL power subsystem" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "Disable SDL sensor subsystem" FORCE)
set(SDL_DIALOG ON CACHE BOOL "Disable SDL dialog subsystem" FORCE)
set(SDL_VULKAN ON CACHE BOOL "Enable SDL Vulkan support" FORCE)

set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build SDL test library" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL examples" FORCE)

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)

add_subdirectory(vendor/sdl3)
add_subdirectory(vendor/nvrhi)
add_subdirectory(vendor/imgui)
add_subdirectory(vendor/stb_image)

target_link_libraries(nvrhi_vk PUBLIC Vulkan::Vulkan)
target_link_libraries(imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan nvrhi nvrhi_vk)

target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC
        SDL3::SDL3
        nvrhi_vk
        nvrhi
        imgui
        stb_image
)

target_compile_definitions(
        ${PROJECT_NAME}
        PUBLIC
        "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1"
)

if(MSVC)
    add_compile_options(/utf-8)
else()
    error("Only MSVC is supported currently")
endif()