# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
project(
    gltf_render_pbr
    LANGUAGES CXX
)

find_package(Vulkan REQUIRED)
find_package(glm)

add_executable(
    ${PROJECT_NAME}
    src/pass/basic_geometry/basic_geometry_pass.cpp
    src/pass/compositing/compositing_pass.cpp
    src/pass/gltf_pbr/gltf_pbr_pass.cpp
    src/pass/gltf_other_channel/gltf_other_channel_pass.cpp
    src/pass/gltf_area_light/gltf_area_light_pass.cpp
    src/engine_layer.cpp
    src/main.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    KDGpu::KDGpuExample
    KDGpu::gltf_holder
    stb
    glm::glm
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_MACOS)
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# DATA ----------------------------------------------------------------------------------------------------------------
add_data_file(${PROJECT_NAME} environment_diffuse environment/diffuse.ktx2
        gltf/gltf_render_pbr/environment
)
add_data_file(${PROJECT_NAME} environment_specular environment/specular.ktx2
        gltf/gltf_render_pbr/environment
)
add_data_file(${PROJECT_NAME} pbr_lut_ggx pbr/lut_ggx.ktx2
        gltf/gltf_render_pbr/pbr
)

add_data_file(${PROJECT_NAME} red_green_blobs_on_white red_green_blobs_on_white.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} small_red_blobs_on_black small_red_blobs_on_black.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_gltf FlightHelmet.gltf
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_bin FlightHelmet.bin
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_01 FlightHelmet_Materials_GlassPlasticMat_BaseColor.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_02 FlightHelmet_Materials_GlassPlasticMat_Normal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_03 FlightHelmet_Materials_GlassPlasticMat_OcclusionRoughMetal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_04 FlightHelmet_Materials_LeatherPartsMat_BaseColor.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_05 FlightHelmet_Materials_LeatherPartsMat_Normal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_06 FlightHelmet_Materials_LeatherPartsMat_OcclusionRoughMetal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_07 FlightHelmet_Materials_LensesMat_BaseColor.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_08 FlightHelmet_Materials_LensesMat_Normal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_09 FlightHelmet_Materials_LensesMat_OcclusionRoughMetal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_10 FlightHelmet_Materials_MetalPartsMat_BaseColor.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_11 FlightHelmet_Materials_MetalPartsMat_Normal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_12 FlightHelmet_Materials_MetalPartsMat_OcclusionRoughMetal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_13 FlightHelmet_Materials_RubberWoodMat_BaseColor.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_14 FlightHelmet_Materials_RubberWoodMat_Normal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} flight_helmet_texture_15 FlightHelmet_Materials_RubberWoodMat_OcclusionRoughMetal.png
        gltf/gltf_render_pbr
)
add_data_file(${PROJECT_NAME} area_light_ltc_amp area_light/ltc_amp_rg32f_32x32.bin
        gltf/gltf_render_pbr/area_light
)
add_data_file(${PROJECT_NAME} area_light_ltc_mat area_light/ltc_mat_rgba32f_32x32.bin
        gltf/gltf_render_pbr/area_light
)
add_data_file(${PROJECT_NAME} textured_stained_glass area_light_textured_light/stained_glass.png
        gltf/gltf_render_pbr/area_light_textured_light
)
add_data_file(${PROJECT_NAME} textured_stained_glass_filtered area_light_textured_light/stained_glass_filtered.png
        gltf/gltf_render_pbr/area_light_textured_light
)



# SHADERS -------------------------------------------------------------------------------------------------------------
compile_shader(${PROJECT_NAME} composite_present_vert
        src/pass/compositing/shader/glsl composite_present.vert.glsl
        gltf/gltf_render_pbr
        vert
)
compile_shader(${PROJECT_NAME} composite_present_frag
        src/pass/compositing/shader/glsl composite_present.frag.glsl
        gltf/gltf_render_pbr
        frag
)

compile_shader(${PROJECT_NAME} gltf_pbr_vert
        src/pass/gltf_pbr/shader/glsl gltf_pbr.vert.glsl
        gltf/gltf_render_pbr
        vert
)
compile_shader(${PROJECT_NAME} gltf_pbr_frag
        src/pass/gltf_pbr/shader/glsl gltf_pbr.frag.glsl
        gltf/gltf_render_pbr
        frag
)

compile_shader(${PROJECT_NAME} gltf_other_channel_vert
        src/pass/gltf_other_channel/shader/glsl gltf_other_channel.vert.glsl
        gltf/gltf_render_pbr
        vert
)
compile_shader(${PROJECT_NAME} gltf_other_channel_frag
        src/pass/gltf_other_channel/shader/glsl gltf_other_channel.frag.glsl
        gltf/gltf_render_pbr
        frag
)

compile_shader(${PROJECT_NAME} basic_geometry_vert
        src/pass/basic_geometry/shader/glsl basic_geometry.vert.glsl
        gltf/gltf_render_pbr
        vert
)
compile_shader(${PROJECT_NAME} basic_geometry_frag
        src/pass/basic_geometry/shader/glsl basic_geometry.frag.glsl
        gltf/gltf_render_pbr
        frag
)

compile_shader(${PROJECT_NAME} gltf_area_light_vert
        src/pass/gltf_area_light/shader/glsl gltf_area_light.vert.glsl
        gltf/gltf_render_pbr
        vert
)
compile_shader(${PROJECT_NAME} gltf_area_light_frag
        src/pass/gltf_area_light/shader/glsl gltf_area_light.frag.glsl
        gltf/gltf_render_pbr
        frag
)

