# linux-capture: the XSHM screen capture keeps capturing the monitor it was
# set to when another monitor is unplugged or plugged in
# (obs-rust/obs-studio#2261). Builds the linux-capture unity file directly, as
# test_xshm_resolution_change does, and adds and removes RandR 1.5 monitors.
# Use a private Xvfb when available; otherwise it must run alone on the
# caller's shared display. Skips without one.
if(OS_LINUX AND TARGET linux-capture AND TARGET OBS::libobs-opengl)
  find_package(X11 REQUIRED)
  find_package(X11_XCB REQUIRED)
  find_package(
    XCB
    REQUIRED XCB XFIXES RANDR SHM XINERAMA COMPOSITE
  )
  add_executable(
    test_xshm_monitor_hotplug
    test_xshm_monitor_hotplug.c
    "${CMAKE_SOURCE_DIR}/plugins/linux-capture/build/linux-capture.nix+.h.c"
  )
  target_include_directories(
    test_xshm_monitor_hotplug
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/plugins/linux-capture"
  )
  target_link_libraries(
    test_xshm_monitor_hotplug
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
  target_compile_definitions(test_xshm_monitor_hotplug PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>")
  add_dependencies(test_xshm_monitor_hotplug libobs-opengl)
  add_dependencies(cmocka-tests test_xshm_monitor_hotplug)
  find_program(OBS_TEST_XVFB_RUN xvfb-run)
  set(_xshm_hotplug_test_display_command "")
  set(_xshm_hotplug_test_serial TRUE)
  if(OBS_TEST_XVFB_RUN)
    set(_xshm_hotplug_test_display_command "${OBS_TEST_XVFB_RUN}" -a "--server-args=-screen 0 1280x1024x24 -noreset")
    set(_xshm_hotplug_test_serial FALSE)
  endif()
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_xshm_monitor_hotplug
    COMMAND ${_xshm_hotplug_test_display_command} $<TARGET_FILE:test_xshm_monitor_hotplug>
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_xshm_monitor_hotplug PROPERTIES TIMEOUT 60 RUN_SERIAL ${_xshm_hotplug_test_serial})
endif()
