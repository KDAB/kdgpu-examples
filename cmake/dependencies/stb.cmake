# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
if(NOT TARGET stb::stb)
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG 5736b15f7ea0ffb08dd38af21067c314d6a3aae9
    )
    FetchContent_MakeAvailable(stb)

    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE
        $<BUILD_INTERFACE:${stb_SOURCE_DIR}>
    )
    add_library(stb::stb ALIAS stb)
endif()
