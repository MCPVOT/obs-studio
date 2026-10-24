# plugins/obs-outputs: the RTMP output's POSIX host (#851).
if(NOT OS_WINDOWS)
  add_executable(test_obs_outputs_rtmp_host test_obs_outputs_rtmp_host.c)
  target_include_directories(test_obs_outputs_rtmp_host PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_obs_outputs_rtmp_host PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_obs_outputs_rtmp_host)
  add_test(test_obs_outputs_rtmp_host ${CMAKE_CURRENT_BINARY_DIR}/test_obs_outputs_rtmp_host)
endif()
