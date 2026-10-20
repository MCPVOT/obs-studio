# plugins/obs-ffmpeg: ffmpeg_output_start() publishes `connecting` before its
# connect worker can clear it (split from #1464). Includes
# obs-ffmpeg-output.c.h with pthread_create replaced; no display needed.
if(OS_LINUX)
  add_executable(test_ffmpeg_output_connecting test_ffmpeg_output_connecting.c)
  target_include_directories(
    test_ffmpeg_output_connecting
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/plugins/obs-ffmpeg"
  )
  target_link_libraries(
    test_ffmpeg_output_connecting
    PRIVATE
      OBS::libobs
      FFmpeg::avcodec
      FFmpeg::avformat
      FFmpeg::avutil
      FFmpeg::swscale
      FFmpeg::swresample
      ${CMOCKA_LIBRARIES}
  )
  add_dependencies(cmocka-tests test_ffmpeg_output_connecting)
  add_test(test_ffmpeg_output_connecting ${CMAKE_CURRENT_BINARY_DIR}/test_ffmpeg_output_connecting)
  set_tests_properties(test_ffmpeg_output_connecting PROPERTIES TIMEOUT 60)
endif()
