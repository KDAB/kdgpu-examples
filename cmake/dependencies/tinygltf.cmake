# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
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
