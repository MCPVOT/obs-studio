# Characterization of media_remux_job_* (Tools > Remux Recordings) before its
# port to obs-media-io: refused jobs, progress, channel layouts. Writes WAV
# inputs by hand and remuxes them with FFmpeg; needs no encoder or display.
add_executable(test_media_remux test_media_remux.c)
target_include_directories(test_media_remux PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(
  test_media_remux
  PRIVATE OBS::libobs FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil ${CMOCKA_LIBRARIES}
)
add_dependencies(cmocka-tests test_media_remux)
add_test(test_media_remux ${CMAKE_CURRENT_BINARY_DIR}/test_media_remux)
set_tests_properties(test_media_remux PROPERTIES TIMEOUT 60)
