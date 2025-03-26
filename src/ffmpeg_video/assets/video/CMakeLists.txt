project(video)

add_custom_target(${PROJECT_NAME} ALL)

set(VIDEO_DIR ${CMAKE_BINARY_DIR}/bin/videos)
file(MAKE_DIRECTORY ${VIDEO_DIR})

set(VIDEO_FILE "${VIDEO_DIR}/big_buck_bunny_720p_h264.mov")

if (NOT EXISTS ${VIDEO_FILE})
    file(DOWNLOAD "https://download.blender.org/peach/bigbuckbunny_movies/big_buck_bunny_720p_h264.mov"
    "${VIDEO_FILE}"
    SHOW_PROGRESS)
endif()

install(
    FILES ${VIDEO_FILES}
    DESTINATION ${CMAKE_INSTALL_BINDIR}/videos
    COMPONENT applications
)
