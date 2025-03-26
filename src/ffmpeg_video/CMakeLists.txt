cmake_minimum_required(VERSION 3.20)

project(ffmpeg-video)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ffmpeg.cmake)

add_subdirectory(assets)

add_executable(
    ${PROJECT_NAME}
    src/main.cpp
    src/video_renderer.h
    src/video_renderer.cpp
    src/video_decoder.h
    src/video_decoder.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC KDGpu::KDGpu KDGpu::KDGpuKDGui FFMpeg::avutil FFMpeg::avcodec FFMpeg::avformat FFMpeg::avdevice FFMpeg::swresample FFMpeg::swscale FFMpeg::avfilter)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FFMPEG_DIR}/bin"
        "${CMAKE_BINARY_DIR}/bin/")
endif()

add_dependencies(${PROJECT_NAME} shaders video)

install(
    TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
