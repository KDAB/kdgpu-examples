# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
project(
    dma_buf_interop
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Vulkan REQUIRED)
find_package(OpenGLES3 REQUIRED)
find_package(GLFW3 QUIET)
if(NOT GLFW3_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        GLFW3
        GIT_REPOSITORY https://github.com/glfw/glfw.git
    )
    FetchContent_MakeAvailable(GLFW3)
endif()

add_executable(
    ${PROJECT_NAME}
    main.cpp renderer.h renderer.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    KDGpu::KDGpuExample
    glfw
    glm::glm
    OpenGLES3::OpenGLES3
    ExampleUtility
)


add_dependencies(
    ${PROJECT_NAME}
    DmaBufInteropShaders
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
