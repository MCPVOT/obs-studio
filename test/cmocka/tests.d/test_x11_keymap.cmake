# X11 hotkey keymap bit test (obs-rust/obs-studio#126). Header-only helper
# that needs only the xcb reply type.
if(OS_LINUX OR OS_FREEBSD)
  add_executable(test_x11_keymap test_x11_keymap.c)
  target_include_directories(test_x11_keymap PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/libobs")
  target_link_libraries(test_x11_keymap PRIVATE ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_x11_keymap)
  add_test(test_x11_keymap ${CMAKE_CURRENT_BINARY_DIR}/test_x11_keymap)
endif()
