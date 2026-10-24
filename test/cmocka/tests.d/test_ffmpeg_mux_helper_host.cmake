# plugins/obs-ffmpeg/ffmpeg-mux: the helper executable's POSIX entry point
# (#853). Plain C, so it builds and runs on every non-Windows test host.
if(NOT OS_WINDOWS)
  add_executable(test_ffmpeg_mux_helper_host test_ffmpeg_mux_helper_host.c)
  target_include_directories(test_ffmpeg_mux_helper_host PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_ffmpeg_mux_helper_host PRIVATE ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_ffmpeg_mux_helper_host)
  add_test(test_ffmpeg_mux_helper_host ${CMAKE_CURRENT_BINARY_DIR}/test_ffmpeg_mux_helper_host)
endif()
