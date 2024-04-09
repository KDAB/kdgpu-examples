# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#

# TODO: we will probably want to have something more verbose in the future
add_custom_command(OUTPUT gltf_renderer_textures
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR} .
)

add_custom_target(gltf_renderer_texture ALL
    DEPENDS gltf_renderer_textures
)
