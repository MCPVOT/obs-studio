if(TARGET OBS::media-playback)
  add_executable(test_media_cached_paused_seek test_media_cached_paused_seek.c)
  target_include_directories(test_media_cached_paused_seek PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(
    test_media_cached_paused_seek
    PRIVATE
      OBS::libobs
      OBS::media-playback
      FFmpeg::swscale
      FFmpeg::swresample
      ${CMOCKA_LIBRARIES}
      $<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:m>
  )
  add_dependencies(cmocka-tests test_media_cached_paused_seek)
  add_test(
    NAME test_media_cached_paused_seek
    COMMAND test_media_cached_paused_seek "${CMAKE_CURRENT_SOURCE_DIR}/data/media_cached_paused_seek.mkv"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )
  set_tests_properties(test_media_cached_paused_seek PROPERTIES TIMEOUT 20)
endif()
