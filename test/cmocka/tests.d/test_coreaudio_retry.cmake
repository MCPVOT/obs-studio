# CoreAudio reconnect backoff schedule (#1358): the helper is plain C with no
# Apple headers (only libobs util), so it builds and runs on every host.
add_executable(test_coreaudio_retry test_coreaudio_retry.c)
target_include_directories(test_coreaudio_retry PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/plugins/mac-capture")
target_link_libraries(test_coreaudio_retry PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
add_dependencies(cmocka-tests test_coreaudio_retry)
add_test(test_coreaudio_retry ${CMAKE_CURRENT_BINARY_DIR}/test_coreaudio_retry)
