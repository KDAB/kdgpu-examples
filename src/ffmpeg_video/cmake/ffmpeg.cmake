# This file is part of KDGpu Examples.
#
# SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#
if(WIN32)
    FetchContent_Declare(
        ffmpeg
        URL https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip
        USES_TERMINAL_DOWNLOAD YES USES_TERMINAL_UPDATE YES
        DOWNLOAD_EXTRACT_TIMESTAMP YES
    )
    FetchContent_MakeAvailable(ffmpeg)

    message(STATUS "Fetching FFMPEG DLLs Win")

    set(FFMPEG_DIR "${ffmpeg_SOURCE_DIR}")
    set(ffmpeg_INCLUDE_DIR "${FFMPEG_DIR}/include")
    set(ffmpeg_LIB_DIR "${FFMPEG_DIR}/lib")
    set(ffmpeg_DLL_DIR "${FFMPEG_DIR}/lib")
    message(STATUS "FFMPEG PATH ${FFMPEG_DIR}")
else()
    set(ffmpeg_INCLUDE_DIR "/usr/include/ffmpeg")
    set(ffmpeg_DLL_DIR "/usr/lib64/")

    if (NOT EXISTS ${ffmpeg_INCLUDE_DIR})
        FetchContent_Declare(
            ffmpeg
            URL https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-linux64-lgpl-shared.tar.xz
            USES_TERMINAL_DOWNLOAD YES USES_TERMINAL_UPDATE YES
            DOWNLOAD_EXTRACT_TIMESTAMP YES
        )
        FetchContent_MakeAvailable(ffmpeg)

        message(STATUS "Fetching FFMPEG libs Linux")

        set(FFMPEG_DIR "${ffmpeg_SOURCE_DIR}")
        message(STATUS "FFMPEG PATH ${FFMPEG_DIR}")

        set(ffmpeg_INCLUDE_DIR "${FFMPEG_DIR}/include")
        set(ffmpeg_DLL_DIR "${FFMPEG_DIR}/lib")
    endif()
endif()


find_path(
    avutil_INCLUDE_DIR
    NAMES avutil.h
    HINTS "${ffmpeg_INCLUDE_DIR}/libavutil")
find_library(
    avutil_DLL_LIBRARY
    NAMES avutil
    HINTS "${ffmpeg_DLL_DIR}")

find_path(
    avcodec_INCLUDE_DIR
    NAMES avcodec.h
    HINTS "${ffmpeg_INCLUDE_DIR}/libavcodec")
find_library(
    avcodec_DLL_LIBRARY
    NAMES avcodec
    HINTS "${ffmpeg_DLL_DIR}")

find_path(
    avformat_INCLUDE_DIR
    NAMES avformat.h
    HINTS "${ffmpeg_INCLUDE_DIR}/libavformat")
find_library(
    avformat_DLL_LIBRARY
    NAMES avformat
    HINTS "${ffmpeg_DLL_DIR}")

find_path(
    avdevice_INCLUDE_DIR
    NAMES avdevice.h
    HINTS "${ffmpeg_INCLUDE_DIR}/libavdevice")
find_library(
    avdevice_DLL_LIBRARY
    NAMES avdevice
    HINTS "${ffmpeg_DLL_DIR}")

find_path(
    swresample_INCLUDE_DIR
    NAMES swresample.h
    HINTS "${ffmpeg_INCLUDE_DIR}/libswresample")
find_library(
    swresample_DLL_LIBRARY
    NAMES swresample
    HINTS "${ffmpeg_DLL_DIR}")

find_path(
    swscale_INCLUDE_DIR
    NAMES swscale.h
    HINTS "${ffmpeg_INCLUDE_DIR}/libswscale")
find_library(
    swscale_DLL_LIBRARY
    NAMES swscale
    HINTS "${ffmpeg_DLL_DIR}")

find_path(
    avfilter_INCLUDE_DIR
    NAMES avfilter.h
    HINTS "${ffmpeg_INCLUDE_DIR}/libavfilter")
find_library(
    avfilter_DLL_LIBRARY
    NAMES avfilter
    HINTS "${ffmpeg_DLL_DIR}")

if(WIN32)
    find_library(
        avutil_IMPORT_LIBRARY
        NAMES avutil
        HINTS "${ffmpeg_LIB_DIR}")
    add_library(FFMpeg::avutil SHARED IMPORTED)
    set_target_properties(FFMpeg::avutil PROPERTIES IMPORTED_LOCATION "${avutil_DLL_LIBRARY}" IMPORTED_IMPLIB "${avutil_IMPORT_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    find_library(
        avcodec_IMPORT_LIBRARY
        NAMES avcodec
        HINTS "${ffmpeg_LIB_DIR}")
    add_library(FFMpeg::avcodec SHARED IMPORTED)
    set_target_properties(FFMpeg::avcodec PROPERTIES IMPORTED_LOCATION "${avcodec_DLL_LIBRARY}" IMPORTED_IMPLIB "${avcodec_IMPORT_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    find_library(
        avformat_IMPORT_LIBRARY
        NAMES avformat
        HINTS "${ffmpeg_LIB_DIR}")
    add_library(FFMpeg::avformat SHARED IMPORTED)
    set_target_properties(FFMpeg::avformat PROPERTIES IMPORTED_LOCATION "${avformat_DLL_LIBRARY}" IMPORTED_IMPLIB "${avformat_IMPORT_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    find_library(
        avdevice_IMPORT_LIBRARY
        NAMES avdevice
        HINTS "${ffmpeg_LIB_DIR}")
    add_library(FFMpeg::avdevice SHARED IMPORTED)
    set_target_properties(FFMpeg::avdevice PROPERTIES IMPORTED_LOCATION "${avdevice_DLL_LIBRARY}" IMPORTED_IMPLIB "${avdevice_IMPORT_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    find_library(
        swresample_IMPORT_LIBRARY
        NAMES swresample
        HINTS "${ffmpeg_LIB_DIR}")
    add_library(FFMpeg::swresample SHARED IMPORTED)
    set_target_properties(FFMpeg::swresample PROPERTIES IMPORTED_LOCATION "${swresample_DLL_LIBRARY}" IMPORTED_IMPLIB "${swresample_IMPORT_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    find_library(
        swscale_IMPORT_LIBRARY
        NAMES swscale
        HINTS "${ffmpeg_LIB_DIR}")
    add_library(FFMpeg::swscale SHARED IMPORTED)
    set_target_properties(FFMpeg::swscale PROPERTIES IMPORTED_LOCATION "${swscale_DLL_LIBRARY}" IMPORTED_IMPLIB "${swscale_IMPORT_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    find_library(
        avfilter_IMPORT_LIBRARY
        NAMES avfilter
        HINTS "${ffmpeg_LIB_DIR}")
    add_library(FFMpeg::avfilter SHARED IMPORTED)
    set_target_properties(FFMpeg::avfilter PROPERTIES IMPORTED_LOCATION "${avfilter_DLL_LIBRARY}" IMPORTED_IMPLIB "${avfilter_IMPORT_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

else()

    add_library(FFMpeg::avutil SHARED IMPORTED)
    set_target_properties(FFMpeg::avutil PROPERTIES IMPORTED_LOCATION "${avutil_DLL_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    add_library(FFMpeg::avcodec SHARED IMPORTED)
    set_target_properties(FFMpeg::avcodec PROPERTIES IMPORTED_LOCATION "${avcodec_DLL_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    add_library(FFMpeg::avformat SHARED IMPORTED)
    set_target_properties(FFMpeg::avformat PROPERTIES IMPORTED_LOCATION "${avformat_DLL_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    add_library(FFMpeg::avdevice SHARED IMPORTED)
    set_target_properties(FFMpeg::avdevice PROPERTIES IMPORTED_LOCATION "${avdevice_DLL_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    add_library(FFMpeg::swresample SHARED IMPORTED)
    set_target_properties(FFMpeg::swresample PROPERTIES IMPORTED_LOCATION "${swresample_DLL_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    add_library(FFMpeg::swscale SHARED IMPORTED)
    set_target_properties(FFMpeg::swscale PROPERTIES IMPORTED_LOCATION "${swscale_DLL_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

    add_library(FFMpeg::avfilter SHARED IMPORTED)
    set_target_properties(FFMpeg::avfilter PROPERTIES IMPORTED_LOCATION "${avfilter_DLL_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_INCLUDE_DIR}")

endif()
