project(
    compute_n_body
    VERSION 0.1
    LANGUAGES CXX
)

find_package(Vulkan REQUIRED)

add_executable(
    ${PROJECT_NAME}
    compute_n_body.cpp main.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    KDGpu::KDGpuExample
    TinyGltfHelper::TinyGltfHelper
    ExampleUtility
    glm::glm
    ktx::ktx
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_MACOS)
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
