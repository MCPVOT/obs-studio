# plugins/vlc-video: the POSIX host leaves playlist paths and URLs as they are
# (#857). Compiles plugins/vlc-video/vlc-video-host-posix.c.h on its own; no
# libVLC needed.
if(NOT OS_WINDOWS AND NOT OS_MACOS)
  add_executable(test_vlc_host_path test_vlc_host_path.c)
  target_include_directories(test_vlc_host_path PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_vlc_host_path PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_vlc_host_path)
  add_test(test_vlc_host_path ${CMAKE_CURRENT_BINARY_DIR}/test_vlc_host_path)
endif()
