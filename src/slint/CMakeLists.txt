# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
project(
    slint-kdgpu
    VERSION 0.1
    LANGUAGES CXX
)

find_package(Vulkan REQUIRED)
find_package(OpenGL REQUIRED)

find_package(Slint QUIET)
if (NOT Slint_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        SOURCE_SUBDIR api/cpp
    )
    FetchContent_MakeAvailable(Slint)
endif (NOT Slint_FOUND)

add_executable(
    ${PROJECT_NAME}
    main.cpp gl_extensions.cpp gl_extensions.h renderer.h renderer.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    Slint::Slint
    OpenGL::GL
    KDGpu::KDGpuExample
    glm::glm
    ExampleUtility
)

slint_target_sources(
    ${PROJECT_NAME}
    app-window.slint
)

add_dependencies(
    ${PROJECT_NAME}
    SlintKdgpuSimpleShaders
)

target_compile_features(
    ${PROJECT_NAME}
    PUBLIC cxx_std_20
)
