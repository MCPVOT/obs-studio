# The linux-v4l2 udev hot-plug thread must stop, not spin, when poll()
# reports an error on its fds (obs-rust/obs-studio#1280). Runs the real loop
# from v4l2-udev.c.h on pipes; needs no udev daemon or camera.
if(OS_LINUX AND TARGET OBS::v4l2)
  find_package(Libudev QUIET)
  if(TARGET Libudev::Libudev)
    add_executable(test_v4l2_udev_poll_error test_v4l2_udev_poll_error.c)
    target_include_directories(
      test_v4l2_udev_poll_error
      PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/plugins/linux-v4l2"
    )
    target_link_libraries(test_v4l2_udev_poll_error PRIVATE OBS::libobs Libudev::Libudev ${CMOCKA_LIBRARIES})
    add_dependencies(cmocka-tests test_v4l2_udev_poll_error)
    add_test(test_v4l2_udev_poll_error ${CMAKE_CURRENT_BINARY_DIR}/test_v4l2_udev_poll_error)
    set_tests_properties(test_v4l2_udev_poll_error PROPERTIES TIMEOUT 60)
  endif()
endif()
