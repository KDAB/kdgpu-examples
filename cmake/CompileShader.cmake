# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
    find_program(
        Vulkan_GLSLANG_VALIDATOR_EXECUTABLE
        NAMES glslangValidator
        HINTS "$ENV{VULKAN_SDK}/bin"
    )
endif()

if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
    message(FATAL_ERROR "glslangValidator executable not found")
endif()

# Compile a shader using glslangValidator
function(CompileShader target shader output)
    # If just the named args are present use them. If there is an optional 4th argument
    # we pass that in too. Useful for passing in -D define options to glslangValidator.
    if(${ARGC} EQUAL 3)
        add_custom_command(OUTPUT ${output}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
            COMMAND ${GLSLANG_VALIDATOR} -V ${CMAKE_CURRENT_SOURCE_DIR}/${shader} -o ${output}
        )
    else()
        list(SUBLIST ARGV 3 -1 REMAINING_ARGS)
        add_custom_command(OUTPUT ${output}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
            COMMAND ${GLSLANG_VALIDATOR} -V ${CMAKE_CURRENT_SOURCE_DIR}/${shader} -o ${output} ${REMAINING_ARGS}
        )
    endif()

    add_custom_target(
        ${target}
        DEPENDS ${output}
        COMMENT "Target to compile a shader"
    )
endfunction()

# Compile raytracing shader using glslangValidator
function(CompileRTShader target shader output)
    add_custom_command(
        OUTPUT ${output}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} --quiet --target-env vulkan1.2
        ${CMAKE_CURRENT_SOURCE_DIR}/${shader} -o ${output}
        COMMENT "Compile shader ${shader} using glslangValidator"
    )

    add_custom_target(
        ${target}
        DEPENDS ${output}
        COMMENT "Target to compile a shader"
    )
endfunction()

# Compile task shader using glslangValidator
function(CompileMeshTaskShader target shader output)
    add_custom_command(
        OUTPUT ${output}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} --quiet --target-env vulkan1.2
        ${CMAKE_CURRENT_SOURCE_DIR}/${shader} -o ${output}
        COMMENT "Compile Mesh/Task shader ${shader} using glslangValidator"
    )

    add_custom_target(
        ${target}
        DEPENDS ${output}
        COMMENT "Target to compile a mesh/task shader"
    )
endfunction()

# Compile s shader set
function(CompileShaderSet target name)
    # TODO: in future we probably want to check which shaders we have instead of assuming vert/frag
    CompileShader(${target}VertexShader ${name}.vert ${name}.vert.spv)
    CompileShader(${target}FragmentShader ${name}.frag ${name}.frag.spv)

    # TODO: for now generate ALL, in future would be better to build on case by case
    add_custom_target(
        ${target}Shaders ALL
        DEPENDS ${target}VertexShader ${target}FragmentShader
        COMMENT "Target to compile a shader set"
    )
endfunction()

# Compile a shader using dxc
function(
    CompileHLSLShader
    target
    shader
    output
    type
)
    add_custom_command(
        OUTPUT ${output}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
        COMMAND ${DXC_EXECUTABLE} -Emain -T${type}_6_1 -Zi $<IF:$<CONFIG:DEBUG>,-Od,-O3> -spirv -Fo${output}
        ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
        COMMENT "Compile shader ${shader} using dxc"
    )

    add_custom_target(
        ${target}
        DEPENDS ${output}
        COMMENT "Target to compile a shader"
    )
endfunction()

# Compiles shader set using dxc
function(CompileHLSLShaderSet target name)
    # TODO: in future we probably want to check which shaders we have instead of assuming vert/frag
    compilehlslshader(${target}VertexShader ${name}.ps.hlsl ${name}.ps.spv ps)
    compilehlslshader(${target}FragmentShader ${name}.vs.hlsl ${name}.vs.spv vs)

    # TODO: for now generate ALL, in future would be better to build on case by case
    add_custom_target(
        ${target}Shaders ALL
        DEPENDS ${target}VertexShader ${target}FragmentShader
        COMMENT "Target to compile a shader set using dxc"
    )
endfunction()

function(CompileShaderVariants target variants_filename)
    # Run the helper script to generate json data for all configured shader variants
    execute_process(
        COMMAND ruby ${CMAKE_SOURCE_DIR}/generate_shader_variants.rb ${variants_filename}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE SHADER_VARIANTS
        RESULT_VARIABLE SHADER_VARIANT_RESULT
    )

    if(NOT SHADER_VARIANT_RESULT EQUAL "0")
        message(NOTICE "generate_shader_variants.rb command returned: ${SHADER_VARIANT_RESULT}")
        message(FATAL_ERROR "Failed to generate shader variant build targets for " ${variants_filename})
    endif()

    string(JSON VARIANT_COUNT LENGTH ${SHADER_VARIANTS} variants)
    message(NOTICE "Generating " ${VARIANT_COUNT} " shader variants from " ${variants_filename})

    # Adjust count as loop index goes from 0 to N
    MATH(EXPR VARIANT_COUNT "${VARIANT_COUNT} - 1")

    foreach(VARIANT_INDEX RANGE ${VARIANT_COUNT})
        string(JSON CURRENT_INTPUT_FILENAME GET ${SHADER_VARIANTS} variants ${VARIANT_INDEX} input)
        string(JSON CURRENT_OUTPUT_FILENAME GET ${SHADER_VARIANTS} variants ${VARIANT_INDEX} output)
        string(JSON CURRENT_DEFINES GET ${SHADER_VARIANTS} variants ${VARIANT_INDEX} defines)

        # message(NOTICE "Adding target for " ${CURRENT_OUTPUT_FILENAME})
        set(SHADER_TARGET_NAME "${target}_${CURRENT_OUTPUT_FILENAME}")
        CompileShader(${SHADER_TARGET_NAME} ${CURRENT_INTPUT_FILENAME} ${CURRENT_OUTPUT_FILENAME} ${CURRENT_DEFINES})

        # TODO: for now generate ALL, in future would be better to build on case by case
        add_custom_target(
            ${SHADER_TARGET_NAME}ShaderVariant ALL
            DEPENDS ${SHADER_TARGET_NAME}
        )
    endforeach(VARIANT_INDEX RANGE ${VARIANT_COUNT})

    # Re-run cmake configure step if the variants file changes
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${variants_filename}
    )
endfunction()
