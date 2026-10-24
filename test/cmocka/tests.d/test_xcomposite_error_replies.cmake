# linux-capture: a failed X request must not leak its xcb error reply
# (obs-rust/obs-studio#2053). Builds the linux-capture unity file directly, as
# test_xcomposite_window_events does. Skips without a display.
if(OS_LINUX AND TARGET linux-capture AND TARGET OBS::libobs-opengl)
  find_package(X11 REQUIRED)
  find_package(X11_XCB REQUIRED)
  find_package(
    XCB
    REQUIRED XCB XFIXES RANDR SHM XINERAMA COMPOSITE
  )
  add_executable(
    test_xcomposite_error_replies
    test_xcomposite_error_replies.c
    "${CMAKE_SOURCE_DIR}/plugins/linux-capture/build/linux-capture.nix+.h.c"
  )
  target_include_directories(
    test_xcomposite_error_replies
    PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/plugins/linux-capture"
  )
  target_link_libraries(
    test_xcomposite_error_replies
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
    test_xcomposite_error_replies
    PRIVATE GRAPHICS_MODULE_PATH="$<TARGET_FILE:OBS::libobs-opengl>"
  )
  add_dependencies(test_xcomposite_error_replies libobs-opengl)
  add_dependencies(cmocka-tests test_xcomposite_error_replies)
  # libobs finds its effect files relative to the working directory.
  add_test(
    NAME test_xcomposite_error_replies
    COMMAND test_xcomposite_error_replies
    WORKING_DIRECTORY "${OBS_OUTPUT_DIR}/$<CONFIG>"
  )
  set_tests_properties(test_xcomposite_error_replies PROPERTIES TIMEOUT 60)
endif()
