# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
CompileShaderVariants(xr_multiview_model_viewer_shaders materials-variants.json)

KDGpu_CompileShaderSet(xr_multiview_model_viewer_solid solid)
KDGpu_CompileShaderSet(xr_multiview_model_viewer_texture texture)
