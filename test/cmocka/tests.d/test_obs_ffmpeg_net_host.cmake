# plugins/obs-ffmpeg: the SRT/URL network host header for POSIX (#853).
if(NOT OS_WINDOWS)
  add_executable(test_obs_ffmpeg_net_host test_obs_ffmpeg_net_host.c)
  target_include_directories(
    test_obs_ffmpeg_net_host
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/plugins/obs-ffmpeg/hosts/posix"
  )
  target_link_libraries(test_obs_ffmpeg_net_host PRIVATE ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_obs_ffmpeg_net_host)
  add_test(test_obs_ffmpeg_net_host ${CMAKE_CURRENT_BINARY_DIR}/test_obs_ffmpeg_net_host)
endif()
