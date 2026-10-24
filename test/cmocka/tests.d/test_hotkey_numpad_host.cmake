# libobs: the hotkey name table's macOS keypad list adds nothing off macOS
# (#870).
if(NOT OS_MACOS)
  add_executable(test_hotkey_numpad_host test_hotkey_numpad_host.c)
  target_include_directories(test_hotkey_numpad_host PRIVATE ${CMOCKA_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}")
  target_link_libraries(test_hotkey_numpad_host PRIVATE ${CMOCKA_LIBRARIES})
  add_dependencies(cmocka-tests test_hotkey_numpad_host)
  add_test(test_hotkey_numpad_host ${CMAKE_CURRENT_BINARY_DIR}/test_hotkey_numpad_host)
endif()
