# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
project(
  06_ffx_cacao
  VERSION 0.1
  LANGUAGES CXX)

find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} main.cpp materials.cpp ffx_cacao.cpp)

target_link_libraries(${PROJECT_NAME} KDGpu::KDGpuExample
                      TinyGltfHelper::TinyGltfHelper ExampleUtility tinygltf stb glm::glm)

target_include_directories(${PROJECT_NAME}
                           PUBLIC ../../../assets/shaders/06_ffx_cacao/)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_MACOS)
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
