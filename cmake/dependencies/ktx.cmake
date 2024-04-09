# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
find_package(ktx 4.0.0 QUIET)

if(NOT TARGET ktx)
    FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG v4.1.0-rc3
    )

    option(KTX_FEATURE_KTX2 "Enable KTX 2 support" ON)
    option(KTX_FEATURE_VULKAN "Enable Vulkan texture upload" ON)
    option(KTX_FEATURE_GL_UPLOAD "Enable OpenGL texture upload" OFF)
    option(KTX_FEATURE_TESTS "Create unit tests" OFF)

    FetchContent_MakeAvailable(ktx)

    target_include_directories(ktx INTERFACE
        $<BUILD_INTERFACE:${ktx_SOURCE_DIR}>
    )

    set_target_properties(ktx
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    add_library(ktx::ktx ALIAS ktx)
endif()
