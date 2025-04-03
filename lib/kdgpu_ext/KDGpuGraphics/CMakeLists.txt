# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#

project(kdgpu_graphics)

find_package(glm)

set(SOURCES
    src/render_target/render_target.cpp
    src/texture_target/texture_target.cpp
    src/texture/single_texture.cpp
    src/texture/texture_set.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES})
add_library(KDGpu::graphics ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC KDGpu::KDGpuExample glm::glm stb::stb)
target_link_libraries(${PROJECT_NAME} PRIVATE ktx::ktx)

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/src
)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
               LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN 1
)

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} BASE_NAME ${PROJECT_NAME})
configure_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_export.h
    ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/${PROJECT_NAME}_export.h
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)

foreach(file ${HEADERS})
    get_filename_component(dir ${file} DIRECTORY)
    install(FILES ${file} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/${dir})
endforeach()
