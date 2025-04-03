project(shaders)

add_custom_target(${PROJECT_NAME} ALL)

KDGpu_CompileShaderSet(textured_quad textured_quad)

add_dependencies(
    ${PROJECT_NAME}
    textured_quadShaders
)

set(SHADER_FILES
    "${CMAKE_CURRENT_BINARY_DIR}/textured_quad.vert.spv"
    "${CMAKE_CURRENT_BINARY_DIR}/textured_quad.frag.spv"
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/shaders)
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_FILES} ${CMAKE_BINARY_DIR}/bin/shaders DEPENDS
    textured_quadShaders
)

install(
    FILES ${SHADER_FILES}
    DESTINATION ${CMAKE_INSTALL_BINDIR}/shaders
    COMPONENT applications
)
