# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#

# KDGpu library

if(NOT TARGET KDGpu::KDGpu)
    find_package(KDGpu CONFIG)
endif()

if(NOT KDGpu_FOUND)
    FetchContent_Declare(
        KDGpu
        GIT_REPOSITORY git@github.com:KDAB/KDGpu.git
        GIT_TAG main
        USES_TERMINAL_DOWNLOAD YES USES_TERMINAL_UPDATE YES
    )

    option(KDGPU_BUILD_TESTS "Build the tests" OFF)
    option(KDGPU_BUILD_EXAMPLES "Build the examples" ON)

    FetchContent_MakeAvailable(KDGpu)
endif()

# Should be available if we have found/built kdgpu above
find_package(KDGpuKDGui CONFIG)
find_package(KDGpuExample CONFIG)
