set(SOURCES
    camera.cpp
    camera_controller.cpp
    camera_controller_layer.cpp
    camera_lens.cpp
    tinygltf_helper.cpp
)

set(HEADERS
    camera.h
    camera_controller.h
    camera_controller_layer.h
    camera_lens.h
    tinygltf_helper.h
)

add_library(
    tinygltf-helper SHARED
    ${SOURCES} ${PUBLIC_HEADERS} ${PRIVATE_HEADERS}
)
add_library(
    TinyGltfHelper::TinyGltfHelper ALIAS tinygltf-helper
)

set(TINYGLTF_HELPER_PUBLIC_LIBS tinygltf KDGpu::KDGpuExample spdlog::spdlog glm::glm)

target_link_libraries(
    tinygltf-helper
    PUBLIC ${TINYGLTF_HELPER_PUBLIC_LIBS}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(tinygltf-helper PRIVATE UNICODE)
    target_compile_definitions(tinygltf-helper PUBLIC _USE_MATH_DEFINES)
endif()

# Require >=C++20 for us and downstream projects
target_compile_features(tinygltf-helper PUBLIC cxx_std_20)

target_include_directories(
    tinygltf-helper
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
           $<INSTALL_INTERFACE:include>
)

set_target_properties(
    tinygltf-helper
    PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
               LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN 1
)

include(GenerateExportHeader)
generate_export_header(tinygltf-helper BASE_NAME tinygltf_helper)
configure_file(
    ${CMAKE_CURRENT_BINARY_DIR}/tinygltf_helper_export.h
    ${CMAKE_BINARY_DIR}/include/tinygltf_helper/tinygltf_helper_export.h
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/tinygltf_helper_export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tinygltf_helper
)

add_feature_info(TinyGltf-Helper ON "Build Library")

foreach(file ${HEADERS})
    get_filename_component(dir ${file} DIRECTORY)
    install(FILES ${file} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tinygltf_helper/${dir})
endforeach()
