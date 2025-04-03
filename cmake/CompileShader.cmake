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

# Compile shader variants
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
        KDGpu_CompileShader(${SHADER_TARGET_NAME} ${CURRENT_INTPUT_FILENAME} ${CURRENT_OUTPUT_FILENAME} ${CURRENT_DEFINES})

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
