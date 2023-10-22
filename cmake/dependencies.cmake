include(FetchContent)

# Note: FetchContent_MakeAvailable builds the project
# if it contains a CMakeLists.txt, otherwise it does nothing.
# ${package_SOURCE_DIR} ${package_BINARY_DIR} are made available by
# MakeAvailable or Populate
message(STATUS "Checking/updating dependencies. This may take a little while...
    Set the FETCHCONTENT_QUIET option to OFF to get verbose output.
")

# KDUtils
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/kdgpu.cmake)

# spdlog Logging Library
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/spdlog.cmake)

# glm
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/glm.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/stb.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/ktx.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/tinygltf.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/sample-models.cmake)
