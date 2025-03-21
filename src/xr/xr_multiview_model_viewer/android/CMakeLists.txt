# This file is part of KDGpu.
#
# SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#

cmake_minimum_required(VERSION 3.18.1)

file(TO_CMAKE_PATH "${ANDROID_NDK}" ANDROID_NDK_PATH)

set(APP_GLUE_DIR ${ANDROID_NDK_PATH}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library(
    app-glue STATIC
    ${APP_GLUE_DIR}/android_native_app_glue.c
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJECT_PATH_MAX 1024)

include(FetchContent)

find_package(KDGpu::KDGpu QUIET)
if(NOT TARGET KDGpu::KDGpu)
    message(STATUS "KDGpu was not found. Fetching from git")
    FetchContent_Declare(
        KDGpu
        GIT_REPOSITORY https://github.com/KDAB/KDGpu.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(KDGpu)
endif()

find_package(tinygltf 2.8.3 QUIET)

if(NOT TARGET tinygltf::tingltf)
    FetchContent_Declare(
            tinygltf
            GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
            GIT_TAG v2.8.3
    )
    FetchContent_Populate(tinygltf)

    option(TINYGLTF_BUILD_LOADER_EXAMPLE "Build loader_example(load glTF and dump infos)" OFF)
    option(TINYGLTF_BUILD_GL_EXAMPLES "Build GL exampels(requires glfw, OpenGL, etc)" OFF)
    option(TINYGLTF_BUILD_VALIDATOR_EXAMPLE "Build validator exampe" OFF)
    option(TINYGLTF_BUILD_BUILDER_EXAMPLE "Build glTF builder example" OFF)
    option(TINYGLTF_HEADER_ONLY "On: header-only mode. Off: create tinygltf library(No TINYGLTF_IMPLEMENTATION required in your project)" ON)
    option(TINYGLTF_INSTALL "Install tinygltf files during install step. Usually set to OFF if you include tinygltf through add_subdirectory()" OFF)

    add_library(tinygltf INTERFACE)
    target_include_directories(tinygltf INTERFACE
            $<BUILD_INTERFACE:${tinygltf_SOURCE_DIR}>
    )
    add_library(tinygltf::tinygltf ALIAS tinygltf)
endif()

project("xr_multiview_model_viewer")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_FIND_DEBUG_MODE TRUE)

add_subdirectory("../../../gltf_renderer/tinygltf_helper" "${CMAKE_BINARY_DIR}/tinygltf_helper_build")

add_library(
    # Sets the name of the library.
    xr_multiview_model_viewer
    # Sets the library as a shared library.
    SHARED
    # Provides a relative path to your source file(s).
    ../main.cpp ../model_scene.cpp ../xr_engine_layer.cpp
)

target_include_directories(
    xr_multiview_model_viewer
    PRIVATE
    "../"
    "../../../utility"
    "tinygltf_helper"
    "${CMAKE_BINARY_DIR}/_deps/glm-src"
    "${CMAKE_BINARY_DIR}/_deps/stb-src"
    "${CMAKE_BINARY_DIR}/_deps/tinygltf-src"
)

target_compile_definitions(
    xr_multiview_model_viewer PRIVATE PLATFORM_ANDROID
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -DVK_USE_PLATFORM_ANDROID_KHR")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

target_link_libraries(
    xr_multiview_model_viewer
    vulkan
    nativewindow
    app-glue
    log
    android
    KDFoundation
    KDGui
    KDUtils
    KDGpu
    imgui
    KDGpuKDGui
    KDGpuExample
    KDGpuExampleResources
    KDGpuExampleShaderResources
    KDXr
    spdlog
    openxr_loader
    tinygltf-helper
)
