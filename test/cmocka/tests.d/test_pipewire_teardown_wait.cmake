# linux-pipewire: tearing down a PipeWire connection never waits on its
# thread loop without the loop lock (#2431). pipewire.c is included by the
# test; no PipeWire daemon, display or D-Bus is needed. Linked with --wrap
# to count unlocked pw_thread_loop_wait() calls.
if(OS_LINUX AND TARGET OBS::pipewire)
  find_package(PipeWire 0.3.60 QUIET)
endif()
if(OS_LINUX AND TARGET OBS::pipewire AND PipeWire_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(_teardown_wait_test_glib REQUIRED IMPORTED_TARGET glib-2.0 gobject-2.0)
  get_target_property(_teardown_wait_drm_includes Libdrm::Libdrm INTERFACE_INCLUDE_DIRECTORIES)
  add_executable(
    test_pipewire_teardown_wait
    test_pipewire_teardown_wait.c
    "${CMAKE_SOURCE_DIR}/plugins/linux-pipewire/formats.c.h"
    "${CMAKE_SOURCE_DIR}/plugins/linux-pipewire/frame-layout.c.h"
    "${CMAKE_SOURCE_DIR}/plugins/linux-pipewire/frame-pacing.c.h"
    "${CMAKE_SOURCE_DIR}/plugins/linux-pipewire/reconnect-policy.c.h"
  )
  target_include_directories(
    test_pipewire_teardown_wait
    PRIVATE
      ${CMOCKA_INCLUDE_DIR}
      "${CMAKE_SOURCE_DIR}"
      "${CMAKE_SOURCE_DIR}/plugins/linux-pipewire"
      ${_teardown_wait_drm_includes}
  )
  target_link_libraries(
    test_pipewire_teardown_wait
    PRIVATE OBS::libobs OBS::glad PipeWire::PipeWire gio::gio PkgConfig::_teardown_wait_test_glib ${CMOCKA_LIBRARIES}
  )
  target_link_options(
    test_pipewire_teardown_wait
    PRIVATE
      "LINKER:--wrap=pw_thread_loop_lock"
      "LINKER:--wrap=pw_thread_loop_unlock"
      "LINKER:--wrap=pw_thread_loop_wait"
  )
  add_dependencies(cmocka-tests test_pipewire_teardown_wait)
  add_test(test_pipewire_teardown_wait ${CMAKE_CURRENT_BINARY_DIR}/test_pipewire_teardown_wait)
endif()
