# plugins/obs-outputs: the network interface list with its POSIX host (#851).
if(NOT OS_WINDOWS)
  add_executable(test_obs_outputs_net_if_host test_obs_outputs_net_if_host.c)
  target_include_directories(
    test_obs_outputs_net_if_host
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/plugins/obs-outputs/hosts/posix"
  )
  target_link_libraries(test_obs_outputs_net_if_host PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_obs_outputs_net_if_host)
  add_test(test_obs_outputs_net_if_host ${CMAKE_CURRENT_BINARY_DIR}/test_obs_outputs_net_if_host)
endif()
