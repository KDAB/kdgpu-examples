# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
CompileShaderSet(ComputeNBody_Particle particle)

CompileShader(ComputeNBody_ParticleCalculate particle_calculate.comp particle_calculate.comp.spv)
add_custom_target(ComputeNBody_ParticleCalculateTmp ALL
    DEPENDS ComputeNBody_ParticleCalculate
)

CompileShader(ComputeNBody_ParticleIntegrate particle_integrate.comp particle_integrate.comp.spv)
add_custom_target(ComputeNBody_ParticleIntegrateTmp ALL
    DEPENDS ComputeNBody_ParticleIntegrate
)
