# plugins/obs-ffmpeg/ffmpeg-mux: the obs-ffmpeg-mux helper executable's seek
# callback, cleanup after a failed open, exit status after a packet error and
# argument ownership when a split recording cannot be opened (split from
# #1464). Includes ffmpeg-mux.c.h with its main() renamed, so its static
# functions are reachable; no display or OBS core is needed.
if(OS_LINUX)
  add_executable(test_ffmpeg_mux_helper test_ffmpeg_mux_helper.c)
  target_include_directories(test_ffmpeg_mux_helper PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(
    test_ffmpeg_mux_helper
    PRIVATE OBS::libobs FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil ${CMOCKA_LIBRARIES}
  )
  add_dependencies(cmocka-tests test_ffmpeg_mux_helper)
  add_test(test_ffmpeg_mux_helper ${CMAKE_CURRENT_BINARY_DIR}/test_ffmpeg_mux_helper)
  set_tests_properties(test_ffmpeg_mux_helper PROPERTIES TIMEOUT 60)
endif()
