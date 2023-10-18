project(shaders)

add_subdirectory(01_simple_gltf)
add_subdirectory(04_instancing)
add_subdirectory(05_materials)
add_subdirectory(compute_n_body)
add_subdirectory(06_ffx_cacao)

add_custom_target(gltf_renderer_assets ALL)

foreach(shader ${SHADER_FILES})
    add_custom_command(TARGET gltf_renderer_assets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/${shader} ${CMAKE_CURRENT_BINARY_DIR}/${shader})
endforeach(shader)
