# This file is part of KDGpu.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
project(
    kdgpu_xr_drawing
    VERSION 0.1
    LANGUAGES CXX
)
find_package(Vulkan REQUIRED)
find_package(nlohmann_json::nlohmann_json)
if(NOT TARGET nlohmann_json::nlohmann_json)
    set(JSON_Install ON)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_kdgpu_example(
    NAME
    ${PROJECT_NAME}
    SOURCES
    main.cpp
    xr_drawing.cpp
    drawing_model.cpp
    xr_drawing_scene.cpp
    imgui_keyboard.cpp
    geometric_primitives.cpp
    LIBS
    KDGpu::KDGpuExample
    nlohmann_json::nlohmann_json
)
