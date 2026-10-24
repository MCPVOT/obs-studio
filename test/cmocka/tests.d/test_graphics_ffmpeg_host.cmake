# libobs/graphics: the image loader's POSIX fallback host (#872).
if(NOT OS_WINDOWS)
  add_executable(test_graphics_ffmpeg_host test_graphics_ffmpeg_host.c)
  target_include_directories(test_graphics_ffmpeg_host PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_graphics_ffmpeg_host PRIVATE OBS::libobs ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_graphics_ffmpeg_host)
  add_test(test_graphics_ffmpeg_host ${CMAKE_CURRENT_BINARY_DIR}/test_graphics_ffmpeg_host)
endif()
