# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
add_subdirectory(gltf_renderer)
add_subdirectory(compute_n_body)
add_subdirectory(utility)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(dma_buf_interop)
endif()

if(KDGPU_BUILD_SLINT_EXAMPLE)
    add_subdirectory(slint)
endif()
