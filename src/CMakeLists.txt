# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group
# company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#

macro(compile_shader project_target target_name source_dir source_file dest_dir stage)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/assets/${dest_dir}/shader)
    add_custom_target(${project_target}_${target_name}
            COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
            -S ${stage}
            -V ${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}/${source_file}
            -o ${CMAKE_BINARY_DIR}/assets/${dest_dir}/shader/${source_file}.spv
    )
    add_dependencies(${project_target} ${project_target}_${target_name})
    install(FILES ${CMAKE_BINARY_DIR}/assets/${dest_dir}/shader/${source_file}.spv
            DESTINATION ${CMAKE_INSTALL_BINDIR}/assets/${dest_dir}/shader)
endmacro()

macro(add_data_file project_target target_name source_file dest_dir)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/assets/${dest_dir})
    add_custom_target(${project_target}_${target_name}
            COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_LIST_DIR}/data/${source_file} ${CMAKE_BINARY_DIR}/assets/${dest_dir}
    )
    add_dependencies(${project_target} ${project_target}_${target_name})

    # extract only the filename from the source file
    set(source_file_v ${source_file})
    cmake_path(GET source_file_v FILENAME source_file_only_filename)

    install(FILES ${CMAKE_BINARY_DIR}/assets/${dest_dir}/${source_file_only_filename}
            DESTINATION ${CMAKE_INSTALL_BINDIR}/assets/${dest_dir})
endmacro()

add_subdirectory(compositing)
add_subdirectory(gltf)
add_subdirectory(gltf_renderer)
add_subdirectory(compute_n_body)
add_subdirectory(utility)
add_subdirectory(ffmpeg_video)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_subdirectory(dma_buf_interop)
endif()

if(KDGPU_BUILD_SLINT_EXAMPLE)
  add_subdirectory(slint)
endif()

if (KDGPU_BUILD_KDXR)
    add_subdirectory(xr)
endif()
