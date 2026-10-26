# CoreAudio zero-channel guard (#2482): the helper is plain C with no Apple
# headers, so it builds and runs on every host.
add_executable(test_coreaudio_zero_channels test_coreaudio_zero_channels.c)
target_include_directories(
  test_coreaudio_zero_channels
  PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/plugins/mac-capture"
)
target_link_libraries(test_coreaudio_zero_channels PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_coreaudio_zero_channels)
add_test(test_coreaudio_zero_channels ${CMAKE_CURRENT_BINARY_DIR}/test_coreaudio_zero_channels)
