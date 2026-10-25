# win-capture: Window Capture's "List OBS windows" option lists and finds
# the windows of OBS's own process (#2226). obs-windows.c.h is a member of the
# plugin's window-capture unity group; the test includes it directly, with the
# plugin's include paths and definitions for its precompiled header.
if(MSVC AND TARGET win-capture)
  add_executable(test_win_capture_obs_windows test_win_capture_obs_windows.c)
  target_include_directories(
    test_win_capture_obs_windows
    PRIVATE
      ${CMOCKA_INCLUDE_DIR}
      "${CMAKE_SOURCE_DIR}"
      "${CMAKE_SOURCE_DIR}/plugins/win-capture"
      $<TARGET_PROPERTY:win-capture,INCLUDE_DIRECTORIES>
  )
  target_compile_definitions(test_win_capture_obs_windows PRIVATE $<TARGET_PROPERTY:win-capture,COMPILE_DEFINITIONS>)
  target_link_libraries(
    test_win_capture_obs_windows
    PRIVATE
      OBS::file-updater
      OBS::hook-config
      OBS::inject-library
      OBS::ipc-util
      OBS::libobs
      OBS::obfuscate
      OBS::w32-pthreads
      OBS::winrt-headers
      jansson::jansson
      ${CMOCKA_LIBRARIES}
  )

  add_dependencies(cmocka-tests test_win_capture_obs_windows)
  add_test(test_win_capture_obs_windows ${CMAKE_CURRENT_BINARY_DIR}/test_win_capture_obs_windows)
endif()
