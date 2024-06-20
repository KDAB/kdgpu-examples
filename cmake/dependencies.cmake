# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group
# company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
include(FetchContent)

# Note: FetchContent_MakeAvailable builds the project if it contains a
# CMakeLists.txt, otherwise it does nothing. ${package_SOURCE_DIR}
# ${package_BINARY_DIR} are made available by MakeAvailable or Populate
message(STATUS "Checking/updating dependencies. This may take a little while...
    Set the FETCHCONTENT_QUIET option to OFF to get verbose output.
")

# kdgpu
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/kdgpu.cmake)

# glm
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/glm.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/stb.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ktx.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/tinygltf.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/sample-models.cmake)
