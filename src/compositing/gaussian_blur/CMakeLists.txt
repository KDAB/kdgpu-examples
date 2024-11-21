# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
project(
    gaussian_blur
    LANGUAGES CXX
)

find_package(Vulkan REQUIRED)
find_package(glm)

add_executable(
    ${PROJECT_NAME}
    src/pass/gaussian_blur/gaussian_blur_render_pass.cpp
    src/pass/geometry/render_geometry_to_texture_render_pass.cpp
    src/pass/texture_to_screen/render_texture_to_screen_render_pass.cpp
    src/engine_layer.cpp
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE KDGpu::graphics)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_MACOS)
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

compile_shader(${PROJECT_NAME} gaussian_blur_vert
        src/pass/gaussian_blur/shader/glsl gaussian_blur.vert.glsl
        compositing/gaussian_blur
        vert
)
compile_shader(${PROJECT_NAME} gaussian_blur_frag
        src/pass/gaussian_blur/shader/glsl gaussian_blur.frag.glsl
        compositing/gaussian_blur
        frag
)
compile_shader(${PROJECT_NAME} rotating_triangle_vert
        src/pass/geometry/shader/glsl rotating_triangle.vert.glsl
        compositing/gaussian_blur
        vert
)
compile_shader(${PROJECT_NAME} rotating_triangle_frag
        src/pass/geometry/shader/glsl rotating_triangle.frag.glsl
        compositing/gaussian_blur
        frag
)
compile_shader(${PROJECT_NAME} texture_to_screen_vert
        src/pass/texture_to_screen/shader/glsl desaturate.vert.glsl
        compositing/gaussian_blur
        vert
)
compile_shader(${PROJECT_NAME} texture_to_screen_frag
        src/pass/texture_to_screen/shader/glsl desaturate.frag.glsl
        compositing/gaussian_blur
        frag
)
