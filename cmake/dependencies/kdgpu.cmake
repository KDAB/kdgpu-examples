# KDGpu library

if(NOT TARGET KDGpu::KDGpu)
    find_package(KDGpu CONFIG)
endif()

if(NOT KDGpu_FOUND)
    FetchContent_Declare(
        KDGpu
        GIT_REPOSITORY ssh://codereview.kdab.com:29418/kdab/toy-renderer
        GIT_TAG main
        USES_TERMINAL_DOWNLOAD YES USES_TERMINAL_UPDATE YES
    )

    option(KDGPU_BUILD_TESTS "Build the tests" OFF)
    option(KDGPU_BUILD_EXAMPLES "Build the examples" ON)

    FetchContent_MakeAvailable(KDGpu)
endif()

# Should be available if we have found/built kdgpu above
find_package(KDGpu_KDGui CONFIG)
