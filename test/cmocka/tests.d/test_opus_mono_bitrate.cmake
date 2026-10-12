# The FFmpeg Opus encoder starts above 256 kbps on a mono track
# (obsproject/obs-studio#12753). Loads the real obs-ffmpeg module and starts
# its encoder through a test output; obs_startup needs a display, so it runs
# (under xvfb-run) on Linux and skips without one.
if(OS_LINUX AND TARGET obs-ffmpeg)
  get_target_property(_obs_ffmpeg_type obs-ffmpeg TYPE)
endif()
if(OS_LINUX AND _obs_ffmpeg_type STREQUAL "MODULE_LIBRARY")
  add_executable(test_opus_mono_bitrate test_opus_mono_bitrate.c)
  target_compile_definitions(
    test_opus_mono_bitrate
    PRIVATE
      FFMPEG_MODULE_PATH="$<TARGET_FILE:obs-ffmpeg>"
      FFMPEG_DATA_PATH="${CMAKE_SOURCE_DIR}/plugins/obs-ffmpeg/data"
  )
  target_include_directories(test_opus_mono_bitrate PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_opus_mono_bitrate PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(test_opus_mono_bitrate obs-ffmpeg)
  add_dependencies(cmocka-tests test_opus_mono_bitrate)
  add_test(NAME test_opus_mono_bitrate COMMAND test_opus_mono_bitrate)
  set_tests_properties(test_opus_mono_bitrate PROPERTIES TIMEOUT 60)
endif()
