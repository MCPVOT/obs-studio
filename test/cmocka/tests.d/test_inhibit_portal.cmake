# Source-only characterization of hidden portal inhibitor entry points.
# The unchanged test_inhibit_sleep_dbus exercises public libobs OFF/ON.
if(OS_LINUX OR OS_FREEBSD)
  find_package(Gio QUIET)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PORTAL_TEST_GLIB QUIET IMPORTED_TARGET glib-2.0 gobject-2.0)
  endif()
  if(TARGET gio::gio AND TARGET PkgConfig::PORTAL_TEST_GLIB)
    add_executable(test_inhibit_portal test_inhibit_portal.c)
    target_include_directories(test_inhibit_portal PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/libobs")
    target_link_libraries(
      test_inhibit_portal
      PRIVATE OBS::libobs gio::gio PkgConfig::PORTAL_TEST_GLIB ${CMOCKA_LIBRARIES}
    )
    add_dependencies(cmocka-tests test_inhibit_portal)
    add_test(test_inhibit_portal ${CMAKE_CURRENT_BINARY_DIR}/test_inhibit_portal)
    set_tests_properties(test_inhibit_portal PROPERTIES TIMEOUT 30)
  endif()
endif()
