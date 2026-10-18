# linux-capture: the XComposite window capture end to end against the Xvfb
# display ctest runs under (obs-rust/obs-studio#1356): a plain xcb window is
# mapped, unmapped, mapped again and destroyed while the real source ticks.
# Builds the linux-capture unity file directly and calls xcomposite_load(), as
# test_media_gif_last_frame builds obs-ffmpeg-source. Skips without a display.
if(OS_LINUX AND TARGET linux-capture AND TARGET OBS::libobs-opengl)
  find_package(X11 REQUIRED)
  find_package(X11_XCB REQUIRED)
  find_package(
    XCB
    REQUIRED XCB XFIXES RANDR SHM XINERAMA COMPOSITE
  )
  add_executable(
    test_xcomposite_window_events
    test_xcomposite_window_events.c
    "${CMAKE_SOURCE_DIR}/plugins/linux-capture/build/linux-capture.nix+.h.c"
  )
  target_include_directories(
    test_xcomposite_window_events
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/plugins/linux-capture"
  )
  target_link_libraries(
    test_xcomposite_window_events
    PRIVATE
      OBS::libobs
      OBS::glad
      X11::X11
      X11::XCB
      XCB::XCB
      XCB::XFIXES
      XCB::RANDR
      XCB::SHM
      XCB::XINERAMA
      XCB::COMPOSITE
      ${CMOCKA_LIBRARIES}
  )
  # A build tree has no libobs-opengl on the loader path, so load it by path.
  target_compile_definitions(
    test_xcomposite_window_events
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  add_dependencies(test_xcomposite_window_events libobs-opengl)
  add_dependencies(cmocka-tests test_xcomposite_window_events)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_xcomposite_window_events
    COMMAND test_xcomposite_window_events
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_xcomposite_window_events PROPERTIES TIMEOUT 60)
endif()
